#include "quart/language/functions.h"
#include <quart/language/state.h>
#include <quart/parser/parser.h>
#include <quart/parser/ast.h>
#include <quart/temporary_change.h>
#include <quart/lexer/lexer.h>

#include <quart/language/trait.h>

#include <quart/stacktrace.h>

namespace quart::ast {

struct ModuleQualifiedName {
    String name;

    explicit ModuleQualifiedName(String name) : name(move(name)) {}
    explicit ModuleQualifiedName() = default;

    operator String() const { return name; }

    void append(String const& segment) {
        name.append("::");
        name.append(segment);
    }
};

static inline bytecode::Register select_dst(State& state, Optional<bytecode::Register> dst) {
    if (dst.has_value()) {
        return dst.value();
    }

    return state.allocate_register();
}

static inline ErrorOr<bytecode::Operand> ensure(State& state, Expr const& expr, Optional<bytecode::Register> dst) {
    auto option = TRY(expr.generate(state, dst));
    if (!option.has_value()) {
        return err(expr.span(), "Expected an expression");
    }

    return option.value();
}

static inline ErrorOr<Vector<GenericTypeParameter>> parse_generic_parameters(State& state, Vector<ast::GenericParameter>const & params) {
    Vector<GenericTypeParameter> parameters;
    for (auto& param : params) {
        Vector<Type*> constraints;
        for (auto& constraint : param.constraints) {
            constraints.push_back(TRY(constraint->evaluate(state)));
        }

        Type* default_type = param.default_type ? TRY(param.default_type->evaluate(state)) : nullptr;
        parameters.push_back({ param.name, constraints, default_type, param.span });
    }

    return parameters;
}

BytecodeResult BlockExpr::generate(State& state, Optional<bytecode::Register>) const {
    for (auto& expr : m_block) {
        TRY(expr->generate(state, {}));
    }

    return {};
}

BytecodeResult ExternBlockExpr::generate(State& state, Optional<bytecode::Register>) const {
    for (auto& expr : m_block) {
        TRY(expr->generate(state, {}));
    }

    return {};
}

BytecodeResult IntegerExpr::generate(State& state, Optional<bytecode::Register>) const {
    IntType* type = nullptr;
    Type* context = state.type_context();

    if (context && context->is_int()) {
        type = context->as<IntType>();
    } else if (m_suffix.type != ast::BuiltinType::None) {
        // We are 100% sure we get an int type from get_type_from_builtin so casting here is ok
        type = state.get_type_from_builtin(m_suffix.type)->as<IntType>();
    } else {
        type = state.context().i32();
    }

    return bytecode::Operand(m_value, type);
}

BytecodeResult StringExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto reg = select_dst(state, dst);
    state.emit<bytecode::NewString>(reg, m_value);

    state.set_register_state(reg, state.context().cstr());
    return bytecode::Operand(reg);
}

BytecodeResult BoolExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto& ctx = state.context();
    
    switch (m_value) {
        case BoolExpr::False:
        case BoolExpr::True:
            return bytecode::Operand(m_value, ctx.i1());
        case BoolExpr::Null: {
            auto reg = select_dst(state, dst);
            Type* type = state.type_context();
            if (!type) {
                type = ctx.void_type()->get_pointer_to();
            }

            state.emit<bytecode::Null>(reg, type);
            state.set_register_state(reg, type);

            return bytecode::Operand(reg);
        }
    }

    ASSERT(false, "Unreachable");
    return {};
}

BytecodeResult ArrayExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    if (m_elements.empty()) {
        return err(span(), "Empty array expressions are not allowed");
    }

    auto& registry = state.context();

    auto reg = select_dst(state, dst);
    Vector<bytecode::Operand> elements;

    quart::Type* array_element_type = nullptr;
    for (auto& expr : m_elements) {
        auto value = TRY(ensure(state, *expr, {}));
        if (elements.empty()) {
            elements.push_back(value);
            array_element_type = state.type(value);

            continue;
        }
    
        value = TRY(state.type_check_and_cast(expr->span(), value, array_element_type, "Array elements must have the same type"));
        elements.emplace_back(value);
    }

    auto* type = registry.create_array_type(array_element_type, elements.size());
    state.emit<bytecode::NewArray>(reg, elements, type);

    state.set_register_state(reg, type);
    return bytecode::Operand(reg);
}

BytecodeResult IdentifierExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto* symbol = state.scope()->resolve(m_name);
    if (!symbol) {
        return err(span(), "Unknown identifier '{}'", m_name);
    }

    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = symbol->as<Variable>();
            auto reg = select_dst(state, dst);

            variable->emit(state, reg);
            return bytecode::Operand(reg);
        }
        case Symbol::Function: {
            auto* function = symbol->as<Function>();
            auto reg = select_dst(state, dst);

            state.emit<bytecode::GetFunction>(reg, function);
            state.set_register_state(reg, function->underlying_type()->get_pointer_to(), function);

            return bytecode::Operand(reg);
        }
        default:
            return err(span(), "'{}' does not refer to a value", m_name);
    }
}

BytecodeResult FloatExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult AssignmentExpr::generate(State& state, Optional<bytecode::Register>) const {
    Optional<bytecode::Operand> value;
    Function* current_function = state.function();

    if (m_value) {
        value = TRY(ensure(state, *m_value, {}));
    }

    Type* type = m_type ? TRY(m_type->evaluate(state)) : nullptr;
    bool is_constructor_value = false;

    if (value) {
        if (!type) {
            type = state.type(*value);
        } else {
            value = TRY(state.type_check_and_cast(span(), *value, type, "Cannot assign a value of type '{}' to a variable of type '{}'"));
        }

        if (value->is_register()) {
            auto& register_state = state.register_state(value->reg());
            is_constructor_value = register_state.flags & RegisterState::Constructor;
    
            if (is_constructor_value) {
                type = type->get_pointee_type();
            }
        }

        if (state.self()) {
            return err(span(), "Cannot assign to a struct method");
        }
    } else {
        if (type->is_reference()) {
            return err(m_identifier.span, "Cannot declare a reference variable without an initializer");
        }
    }

    if (!type->is_sized_type()) {
        if (m_value) {
            return err(m_value->span(), "Cannot assign value of unsized type '{}'", type->str());
        } else {
            return err(m_type->span(), "Cannot declare variable of unsized type '{}'", type->str());
        }
    }

    size_t local_index = current_function->allocate_local();
    if (is_constructor_value) {
        current_function->add_struct_local(local_index);
    }

    current_function->set_local_type(local_index, type);

    u8 flags = Variable::None;
    if (m_identifier.is_mutable) {
        flags |= Variable::Mutable;
    } if (m_is_public) {
        flags |= Variable::Public;
    }

    auto variable = Variable::create(m_identifier.value, local_index, type, flags);
    variable->set_module(state.module());

    state.emit<bytecode::SetLocal>(local_index, value);
    state.scope()->add_symbol(variable);

    return {};
}

BytecodeResult TupleAssignmentExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult ConstExpr::generate(State& state, Optional<bytecode::Register>) const {
    Constant* constant = TRY(state.constant_evaluator().evaluate(*m_value));
    size_t global_index = state.allocate_global();

    u8 flags = Variable::Constant;
    if (m_is_public) {
        flags |= Variable::Public;
    }

    auto variable = Variable::create(m_name, global_index, constant->type(), flags);
    variable->set_module(state.module());

    variable->set_initializer(constant);

    state.scope()->add_symbol(variable);
    state.add_global(move(variable));

    return {};
}

BytecodeResult UnaryOpExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto reg = select_dst(state, dst);
    switch (m_op) {
        case UnaryOp::Not: {
            bytecode::Operand value = TRY(ensure(state, *m_value, {}));
            state.emit<bytecode::Not>(reg, value);

            state.set_register_state(reg, state.context().i1());
            return bytecode::Operand(reg);
        }
        case UnaryOp::DeRef: {
            bytecode::Operand value = TRY(ensure(state, *m_value, {}));
            Type* type = state.type(value);

            if (!type->is_pointer() && !type->is_reference()) {
                return err(span(), "Cannot de-reference value of type '{}'", type->str());
            }

            state.emit<bytecode::Read>(reg, value.reg());
            state.set_register_state(reg, type->underlying_type());

            return bytecode::Operand(reg);
        }
        default:
            ASSERT(false, "Unimplemented");
    }

    return {};
}

BytecodeResult BinaryOpExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    if (m_op == BinaryOp::Assign) {
        if (m_lhs->is<UnaryOpExpr>()) {
            auto* unary = m_lhs->as<UnaryOpExpr>();

            if (unary->op() == UnaryOp::DeRef) {
                auto& value = unary->value();

                auto lhs = TRY(ensure(state, value, {}));
                Type* type = state.type(lhs);

                if (!type->is_pointer() && !type->is_reference()) {
                    return err(value.span(), "Cannot dereference a value of type '{}'", type->str());
                }

                if (!type->is_mutable()) {
                    return err(value.span(), "Cannot assign to a non-mutable reference");
                }

                auto rhs = TRY(ensure(state, *m_rhs, {}));
                rhs = TRY(state.type_check_and_cast(m_rhs->span(), rhs, type->underlying_type(), "Cannot assign a value of type '{}' to a variable of type '{}'"));

                state.emit<bytecode::Write>(lhs.reg(), rhs);
                return {};
            }

            return err(span(), "Invalid left-hand side of assignment");
        }

        auto lhs = TRY(state.resolve_reference(*m_lhs, true));
        auto rhs = TRY(ensure(state, *m_rhs, {}));

        Type* lhs_type = state.type(lhs)->get_reference_type();
        rhs = TRY(state.type_check_and_cast(m_lhs->span(), rhs, lhs_type, "Cannot assign a value of type '{}' to a variable of type '{}'"));

        state.emit<bytecode::Write>(lhs, rhs);
        return {};
    }

    bytecode::Operand lhs = TRY(ensure(state, *m_lhs, {}));
    Type* lhs_type = state.type(lhs);

    state.set_type_context(lhs_type);
    bytecode::Operand rhs = TRY(ensure(state, *m_rhs, {}));

    rhs = TRY(state.type_check_and_cast(span(), rhs, lhs_type, "Cannot perform binary operation on operands of type '{}' and '{}'"));

    auto reg = select_dst(state, dst);
    switch (m_op) {
        // NOLINTNEXTLINE
        #define Op(x) case BinaryOp::x: state.emit<bytecode::x>(reg, lhs, rhs); break;
            ENUMERATE_BINARY_OPS(Op)
        #undef Op

        default:
            return err(span(), "Unknown binary operator");
    }

    if (is_comparison_operator(m_op)) {
        state.set_register_state(reg, state.context().i1());
    } else {
        state.set_register_state(reg, lhs_type);
    }

    state.set_type_context(nullptr);
    return bytecode::Operand(reg);
}

BytecodeResult InplaceBinaryOpExpr::generate(State& state, Optional<bytecode::Register>) const {
    auto ref = TRY(state.resolve_reference(*m_lhs, true));
    Type* type = state.type(ref)->get_reference_type();

    auto lhs = state.allocate_register();
    state.emit<bytecode::Read>(lhs, ref);

    auto rhs = TRY(ensure(state, *m_rhs, {}));
    rhs = TRY(state.type_check_and_cast(span(), rhs, type, "Cannot assign a value of type '{}' to a variable of type '{}'"));

    auto reg = state.allocate_register();
    switch (m_op) {
        // NOLINTNEXTLINE
        #define Op(x) case BinaryOp::x: state.emit<bytecode::x>(reg, lhs, rhs); break;
            ENUMERATE_BINARY_OPS(Op)
        #undef Op

        default:
            return err(span(), "Unknown binary operator");
    }

    state.emit<bytecode::Write>(ref, reg);
    return {};
}

BytecodeResult ReferenceExpr::generate(State& state, Optional<bytecode::Register>) const {
    auto reg = TRY(state.resolve_reference(*m_value, m_is_mutable));
    return bytecode::Operand(reg);
}

static ErrorOr<void> generate_generic_function_call(
    State& state,
    Vector<bytecode::Operand>& arguments,
    FunctionType const* function_type, 
    Vector<OwnPtr<Expr>> const& args,
    size_t index,
    size_t params
) {
    for (auto& arg : args) {
        if (index >= params && function_type->is_var_arg()) {
            auto operand = TRY(ensure(state, *arg, {}));
            arguments.push_back(operand);

            continue;
        }

        Type* parameter_type = function_type->get_parameter_at(index);
        state.set_type_context(parameter_type);

        auto operand = TRY(ensure(state, *arg, {}));

        operand = TRY(state.type_check_and_cast(arg->span(), operand, parameter_type, "Cannot pass a value of type '{}' to a parameter that expects '{}'"));
        arguments.push_back(operand);

        state.set_type_context(nullptr);
        index++;
    }

    return {};
}

static ErrorOr<void> generate_function_call(
    State& state,
    Vector<bytecode::Operand>& arguments,
    Function* function,
    FunctionType const* function_type,
    Vector<OwnPtr<Expr>> const& args,
    size_t index,
    size_t params
) {
    for (auto& arg : args) {
        if (index >= params && function_type->is_var_arg()) {
            auto operand = TRY(ensure(state, *arg, {}));
            arguments.push_back(operand);

            continue;
        }

        FunctionParameter const& parameter = function->parameters()[index];
        if (!parameter.is_byval()) {
            state.set_type_context(parameter.type);

            auto operand = TRY(ensure(state, *arg, {}));
            if (state.self()) {
                return err(arg->span(), "Cannot use a struct method as a value");
            }

            operand = TRY(state.type_check_and_cast(arg->span(), operand, parameter.type, "Cannot pass a value of type '{}' to a parameter that expects '{}'"));
            arguments.push_back(operand);

            state.set_type_context(nullptr);
            index++;

            continue;
        }

        Type* underlying_type = parameter.type->get_pointee_type();
        auto result = state.resolve_reference(*arg, false, {}, false);

        bytecode::Register reg = state.allocate_register();
        state.set_register_state(reg, parameter.type);

        if (result.is_err()) {
            auto operand = TRY(ensure(state, *arg, {}));
            Type* type = state.type(operand);

            if (type != underlying_type) {
                return err(arg->span(), "Cannot pass a value of type '{}' to a parameter that expects '{}'", type->str(), underlying_type->str());
            }

            state.emit<bytecode::Alloca>(reg, underlying_type);
            state.emit<bytecode::Write>(reg, operand);
        } else {
            bytecode::Register src = result.value();

            state.emit<bytecode::Alloca>(reg, underlying_type);
            state.emit<bytecode::Memcpy>(reg, src, underlying_type->size());
        }

        arguments.emplace_back(reg);
        index++;
    }

    return {};
}

static ErrorOr<bytecode::Operand> generate_trait_call_argument(
    State& state,
    FunctionParameter const& parameter,
    Expr const& argument
) {
    auto operand = TRY(ensure(state, argument, {}));
    if (state.self()) {
        return err(argument.span(), "Cannot use a struct method as a value");
    }

    bool is_trait_type = parameter.type->is_underlying_type_of(quart::TypeKind::Trait);
    if (is_trait_type) {
        Type* ty = state.type(operand);
        if (!ty->is_pointer() && !ty->is_reference()) {
            return err(argument.span(), "Cannot pass a value of type '{}' to a parameter that expects '{}'", ty->str(), parameter.type->str());
        }

        ty = ty->underlying_type();
        auto* trait_type = parameter.type->underlying_type()->as<TraitType>();

        if (!ty->is<StructType>()) { // TODO: Allow non-struct types to implement traits
            return err(argument.span(), "Type '{}' does not implement trait '{}'", ty->str(), parameter.type->str());
        }

        auto structure = ty->as<StructType>()->get_struct();
        if (!structure->impls_trait(trait_type)) {
            return err(argument.span(), "Type '{}' does not implement trait '{}'", ty->str(), parameter.type->str());
        }

        return operand;
    }

    return TRY(state.type_check_and_cast(argument.span(), operand, parameter.type, "Cannot pass a value of type '{}' to a parameter that expects '{}'"));
}

static ErrorOr<RefPtr<Function>> generate_trait_function_call(
    State& state,
    Vector<bytecode::Operand>& arguments,
    Function* function,
    FunctionType const* function_type,
    Vector<OwnPtr<Expr>> const& args,
    size_t index,
    size_t params
) {
    Vector<FunctionParameter> parameters;
    for (auto& arg : args) {
        if (index >= params && function_type->is_var_arg()) {
            auto operand = TRY(ensure(state, *arg, {}));
            arguments.push_back(operand);

            continue;
        }

        FunctionParameter const& parameter = function->parameters()[index];
        if (!parameter.is_byval()) {
            state.set_type_context(parameter.type);

            auto operand = TRY(generate_trait_call_argument(state, parameter, *arg));

            arguments.push_back(operand);
            parameters.push_back(parameter.clone(state.type(operand)));

            state.set_type_context(nullptr);
            index++;

            continue;
        }

        Type* underlying_type = parameter.type->get_pointee_type();
        auto result = state.resolve_reference(*arg, false, {}, false);

        bytecode::Register reg = state.allocate_register();
        state.set_register_state(reg, parameter.type);

        if (result.is_err()) {
            auto operand = TRY(ensure(state, *arg, {}));
            Type* type = state.type(operand);

            if (type != underlying_type) {
                return err(arg->span(), "Cannot pass a value of type '{}' to a parameter that expects '{}'", type->str(), underlying_type->str());
            }

            state.emit<bytecode::Alloca>(reg, underlying_type);
            state.emit<bytecode::Write>(reg, operand);
        } else {
            bytecode::Register src = result.value();

            state.emit<bytecode::Alloca>(reg, underlying_type);
            state.emit<bytecode::Memcpy>(reg, src, underlying_type->size());
        }

        arguments.emplace_back(reg);
        parameters.push_back(parameter.clone(state.type(reg)));

        index++;
    }

    return TRY(function->specialize(state, parameters));
}

BytecodeResult CallExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    bytecode::Operand callee = TRY(ensure(state, *m_callee, {}));
    ASSERT(callee.is_register(), "Callee must be a register");

    auto& register_state = state.register_state(callee.reg());

    Type* type = register_state.type;
    Function* function = register_state.function;

    FunctionType const* function_type = nullptr;
    if (type->is_pointer()) {
        Type* pointee = type->get_pointee_type();
        if (!pointee->is_function()) {
            return err(span(), "Cannot call a value of type '{}'", type->str());
        }

        function_type = pointee->as<FunctionType>();
    } else if (type->is_function()) {
        function_type = type->as<FunctionType>();
    } else {
        return err(span(), "Cannot call a value of type '{}'", type->str());
    }

    Optional<bytecode::Register> self = state.self();

    size_t index = 0;
    size_t params = function_type->parameters().size();

    // The only case in which `self` would be not None is when calling a method so we know for sure that we can just ignore the first parameter.
    if (self.has_value()) {
        params--;
        index++;
    }

    if (function_type->is_var_arg() && m_args.size() < params) {
        return err(span(), "Expected at least {} arguments but got {}", params, m_args.size());
    } else if (!function_type->is_var_arg() && m_args.size() != params) {
        return err(span(), "Expected {} arguments but got {}", params, m_args.size());
    }

    Vector<bytecode::Operand> arguments;
    if (self.has_value()) {
        arguments.emplace_back(self.value());
        state.reset_self();
    }

    Optional<bytecode::Register> constructor_register = {};
    if (function) {
        if (function->has_trait_parameter()) {
            auto specialized = TRY(generate_trait_function_call(state, arguments, function, function_type, m_args, index, params));

            auto reg = state.allocate_register();
            state.emit<bytecode::GetFunction>(reg, specialized.get());

            auto return_register = select_dst(state, dst);
            state.emit<bytecode::Call>(return_register, reg, specialized->underlying_type(), arguments);

            state.set_register_state(return_register, specialized->return_type());
            return bytecode::Operand(return_register);
        }

        if (function->is_struct_return()) {
            auto return_register = state.return_register();
            if (return_register.has_value()) {
                arguments.emplace_back(*return_register);
            } else {
                constructor_register = state.allocate_register();
                state.emit<bytecode::Alloca>(*constructor_register, function->return_type());

                state.set_register_state(*constructor_register, function->return_type()->get_pointer_to(), nullptr, RegisterState::Constructor);
                arguments.emplace_back(*constructor_register);
            }
        }

        TRY(generate_function_call(state, arguments, function, function_type, m_args, index, params));
    } else {
        TRY(generate_generic_function_call(state, arguments, function_type, m_args, index, params));
    }

    auto reg = select_dst(state, dst);
    state.emit<bytecode::Call>(reg, callee.reg(), function_type, arguments);

    if (constructor_register.has_value()) {
        return bytecode::Operand(constructor_register.value());
    }

    state.set_register_state(reg, function_type->return_type());
    return bytecode::Operand(reg);
}

BytecodeResult ReturnExpr::generate(State& state, Optional<bytecode::Register>) const {
    Function* current_function = state.function();

    Type* return_type = current_function->return_type();
    if (m_value) {
        if (return_type->is_void()) {
            return err(m_value->span(), "Cannot return a value from a function that expects void");
        }

        auto operand = TRY(ensure(state, *m_value, {}));
        if (operand.is_register()) {
            auto& register_state = state.register_state(operand.reg());
            if (register_state.flags & RegisterState::Constructor || state.return_register().has_value()) {
                state.emit<bytecode::Return>();
                return {};
            }
        }

        operand = TRY(state.type_check_and_cast(m_value->span(), operand, return_type, "Cannot return a value of type '{}' from a function that expects '{}'"));
        state.emit<bytecode::Return>(operand);
    } else {
        if (!return_type->is_void()) {
            return err(span(), "Cannot return void from a function that expects '{}'", return_type->str());
        }

        state.emit<bytecode::Return>();
    }

    return {};
}

BytecodeResult FunctionDeclExpr::generate(State& state, Optional<bytecode::Register>) const {
    Vector<FunctionParameter> parameters;
    Type* self_type = state.self_type();

    for (auto [index, param] : llvm::enumerate(m_parameters)) {
        Type* type = nullptr;
        u8 flags = param.flags;

        if (self_type && flags & FunctionParameter::Self) {
            type = self_type->get_pointer_to(flags & FunctionParameter::Mutable);
        } else {
            type = TRY(param.type->evaluate(state));
        }

        if (!type->is_sized_type()) {
            return err(param.span, "Parameter '{}' of type '{}' has no size. Consider using a pointer or reference", param.name, type->str());
        }

        if (type->is_reference()) {
            bool is_mutable = flags & FunctionParameter::Mutable;
            if (type->is_mutable() && !is_mutable) {
                flags |= FunctionParameter::Mutable;
            } else if (is_mutable && !type->is_mutable()) {
                return err(param.span, "Cannot declare a mutable parameter that takes an immutable reference");
            }
        }

        if (type->is_aggregate()) {
            flags |= FunctionParameter::Byval;
            type = type->get_pointer_to();
        }

        parameters.push_back({ param.name, type, flags, static_cast<u32>(index), param.span });
    }

    Type* return_type = state.context().void_type();
    if (m_return_type) {
        return_type = TRY(m_return_type->evaluate(state));
    }

    auto range = llvm::map_range(parameters, [](auto& param) { return param.type; });
    auto params = Vector<Type*>(range.begin(), range.end());

    auto* underlying_type = FunctionType::get(state.context(), return_type, params, m_is_c_variadic);

    auto scope = Scope::create(m_name, ScopeType::Function, state.scope());

    RefPtr<LinkInfo> link_info = nullptr;
    if (m_attrs.has(Attribute::Link)) {
        auto& attr = m_attrs[Attribute::Link];
        link_info = attr.value<RefPtr<LinkInfo>>();
    }

    auto function = Function::create(
        span(),
        m_name,
        parameters,
        underlying_type,
        scope,
        m_linkage,
        move(link_info),
        m_is_public,
        m_is_async
    );

    function->set_module(state.module());
    if (auto* original = state.get_global_function(function->qualified_name())) {
        auto error = err(span(), "Function '{}' is already defined", function->qualified_name());
        error.add_note(original->span(), "Previous definition is here");

        return error;
    }

    state.scope()->add_symbol(function);
    state.add_global_function(function);

    state.emit<bytecode::NewFunction>(&*function);
    return {};
}

BytecodeResult FunctionExpr::generate(State& state, Optional<bytecode::Register>) const {
    TRY(m_decl->generate(state, {}));
    auto* function = state.scope()->resolve<Function>(m_decl->name());

    auto* previous_function = state.function();
    auto previous_scope = state.scope();

    if (function->has_trait_parameter()) {
        state.set_current_function(function);
        state.set_current_scope(function->scope());

        function->set_local_parameters();

        TRY(state.type_checker().type_check(*m_body));

        state.set_current_function(previous_function);
        state.set_current_scope(previous_scope);

        function->set_body(m_body.get());
        return {};
    }

    auto* entry_block = state.create_block();
    function->set_entry_block(entry_block);
    
    auto* previous_block = state.current_block();
    state.switch_to(entry_block);

    function->set_local_parameters();

    state.set_current_scope(function->scope());
    state.set_current_function(function);

    state.emit<bytecode::NewLocalScope>(function);
    function->set_is_decl(false);

    if (function->is_struct_return()) {
        auto return_register = state.allocate_register();
        state.emit<bytecode::GetReturn>(return_register);

        state.set_register_state(return_register, function->return_type()->get_pointer_to());
        state.inject_return(return_register);
    }

    TRY(m_body->generate(state, {}));
    TRY(function->finalize_body(state));

    state.switch_to(previous_block);
    state.set_current_scope(previous_scope);

    state.set_current_function(previous_function);
    state.reset_return();

    return {};
}

BytecodeResult DeferExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult IfExpr::generate(State& state, Optional<bytecode::Register>) const {
    Function* current_function = state.function();
    if (!current_function) {
        return err(span(), "If statements are not allowed outside functions");
    }

    auto* then_block = state.create_block();
    auto* else_block = state.create_block();

    auto operand = TRY(ensure(state, *m_condition, {}));
    operand = TRY(state.type_check_and_cast(m_condition->span(), operand, state.context().i1(), "If conditions must be booleans"));

    state.emit<bytecode::JumpIf>(operand, then_block, else_block);

    current_function->insert_block(then_block);
    current_function->insert_block(else_block);

    state.switch_to(then_block);
    TRY(m_body->generate(state, {}));

    if (m_else_body) {
        bytecode::BasicBlock* end_block = nullptr;
        if (!then_block->is_terminated()) {
            end_block = state.create_block();
            state.emit<bytecode::Jump>(end_block);
        }

        state.switch_to(else_block);
        TRY(m_else_body->generate(state, {}));

        if (end_block) {
            if (!else_block->is_terminated()) {
                state.emit<bytecode::Jump>(end_block);
            }
            
            state.switch_to(end_block);
            current_function->insert_block(end_block);
        }
    } else {
        if (!then_block->is_terminated()) {
            state.emit<bytecode::Jump>(else_block);
        }

        state.switch_to(else_block);
    }

    return {};
}

BytecodeResult WhileExpr::generate(State& state, Optional<bytecode::Register>) const {
    Function* current_function = state.function();
    if (!current_function) {
        return err(span(), "While loops are not allowed outside functions");
    }

    auto operand = TRY(ensure(state, *m_condition, {}));
    operand = TRY(state.type_check_and_cast(m_condition->span(), operand, state.context().i1(), "While conditions must be booleans"));

    auto* while_block = state.create_block();
    auto* end_block = state.create_block();

    TemporaryChange<Loop> change(current_function->current_loop(), { while_block, end_block });

    state.emit<bytecode::JumpIf>(operand, while_block, end_block);
    current_function->insert_block(while_block);

    state.switch_to(while_block);
    TRY(m_body->generate(state, {}));

    operand = TRY(ensure(state, *m_condition, {}));
    state.emit<bytecode::JumpIf>(operand, while_block, end_block);

    current_function->insert_block(end_block);
    state.switch_to(end_block);

    return {};
}

BytecodeResult BreakExpr::generate(State& state, Optional<bytecode::Register>) const {
    Function* current_function = state.function();
    auto& current_loop = current_function->current_loop();

    state.emit<bytecode::Jump>(current_loop.end);
    return {};
}

BytecodeResult ContinueExpr::generate(State& state, Optional<bytecode::Register>) const {
    Function* current_function = state.function();
    auto& current_loop = current_function->current_loop();

    state.emit<bytecode::Jump>(current_loop.start);
    return {};
}

BytecodeResult StructExpr::generate(State& state, Optional<bytecode::Register>) const {
    if (m_opaque) {
        auto* type = StructType::get(state.context(), Symbol::parse_qualified_name(m_name, state.scope()), {});
        auto structure = Struct::create(m_name, type, state.scope(), m_is_public);

        state.scope()->add_symbol(structure);
        state.emit<bytecode::NewStruct>(&*structure);

        structure->set_module(state.module());
        return {};
    }

    auto* type = StructType::get(state.context(), Symbol::parse_qualified_name(m_name, state.scope()), {});
    auto scope = Scope::create(m_name, ScopeType::Struct, state.scope());

    auto structure = Struct::create(m_name, type, {}, scope, m_is_public);
    structure->set_module(state.module());

    type->set_struct(&*structure);
    state.scope()->add_symbol(structure);

    HashMap<String, quart::StructField> fields;
    Vector<Type*> types;

    for (auto& field : m_fields) {
        Type* type = TRY(field.type->evaluate(state));
        if (!type->is_sized_type()) {
            return err(field.type->span(), "Field '{}' has an unsized type", field.name);
        } else if (type == structure->underlying_type()) {
            return err(field.type->span(), "Field '{}' has the same type as the struct itself", field.name);
        }

        fields.insert_or_assign(field.name, quart::StructField { field.name, type, field.flags, field.index });
        types.push_back(type);
    }

    type->set_fields(types);
    structure->set_fields(move(fields));

    auto previous_scope = state.scope();

    state.set_current_scope(scope);
    state.set_current_struct(structure.get());

    state.add_global_struct(structure);
    state.set_self_type(structure->underlying_type());

    state.emit<bytecode::NewStruct>(structure.get());
    for (auto& expr : m_members) {
        TRY(expr->generate(state, {}));
    }

    state.set_current_scope(previous_scope);
    state.set_self_type(nullptr);
    state.set_current_struct(nullptr);
    
    return {};
}

BytecodeResult ConstructorExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    Struct* structure = TRY(state.resolve_struct(*m_parent));
    auto& fields = structure->fields();

    Vector<bytecode::Operand> arguments;
    arguments.resize(fields.size());

    for (auto& argument : m_arguments) {
        auto iterator = fields.find(argument.name);
        if (iterator == fields.end()) {
            return err(argument.span, "Unknown field '{}' for struct '{}'", argument.name, structure->name());
        }

        auto& field = iterator->second;
        state.set_type_context(field.type);

        auto value = TRY(ensure(state, *argument.value, {}));

        value = TRY(state.type_check_and_cast(argument.value->span(), value, field.type, "Cannot assign a value of type '{}' to a field of type '{}'"));
        arguments[field.index] = value;

        state.set_type_context(nullptr);
    }

    auto return_value = state.return_register();
    if (return_value.has_value()) {
        auto& register_state = state.register_state(*return_value);
        auto* type = register_state.type->get_pointee_type();

        if (type == structure->underlying_type()) {
            auto reg = *return_value;

            for (size_t i = 0; i < arguments.size(); i++) {
                bytecode::Operand index { i, state.context().i32() };
                state.emit<bytecode::SetMember>(reg, index, arguments[i]);
            }

            state.set_register_flags(reg, RegisterState::Constructor);
            return bytecode::Operand(reg);
        }
    }

    auto reg = select_dst(state, dst);
    state.emit<bytecode::Construct>(reg, structure, arguments);

    state.set_register_state(reg, structure->underlying_type());
    return bytecode::Operand(reg);
}

BytecodeResult EmptyConstructorExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult AttributeExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto reg = TRY(state.generate_attribute_access(*this, false, false, dst));
    return bytecode::Operand(reg);
}

BytecodeResult IndexExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto reg = TRY(state.generate_index_access(*this, false, false, dst));
    return bytecode::Operand(reg);
}

BytecodeResult CastExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto value = TRY(ensure(state, *m_value, {}));
    Type* type = TRY(m_to->evaluate(state));

    // FIXME: More checks are needed to be put in place and we can't really use State::type_check_and_cast here
    // because it does a "safe" cast and this is more of a "force" cast.

    // Type* from = state.type(value);
    // if (type->is_mutable() && !from->is_mutable()) {
    //     return err(span(), "Cannot cast a non-mutable value to a mutable value");
    // }

    auto reg = select_dst(state, dst);
    state.emit<bytecode::Cast>(reg, value, type);

    state.set_register_state(reg, type);
    return bytecode::Operand(reg);
}

BytecodeResult SizeofExpr::generate(State& state, Optional<bytecode::Register>) const {
    size_t size = TRY(state.size_of(*m_value));
    Type* context = state.type_context();

    Type* type = nullptr;
    if (context && context->is_int()) {
        type = context->as<IntType>();
    } else {
        type = state.context().u32();
    }

    return bytecode::Operand(size, type);
}

BytecodeResult OffsetofExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult PathExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto scope = TRY(state.resolve_scope_path(span(), m_path));
    auto* symbol = scope->resolve(m_path.name());

    if (!symbol) {
        return err(span(), "Unknown identifier '{}'", m_path.format());
    }

    if (!symbol->is_public() && symbol->module() != state.module()) {
        return err(span(), "Cannot access private symbol '{}'", m_path.format());
    }

    auto reg = select_dst(state, dst);
    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = symbol->as<Variable>();
            variable->emit(state, reg);
            
            return bytecode::Operand(reg);
        }
        case Symbol::Function: {
            auto* function = symbol->as<Function>();
            state.emit<bytecode::GetFunction>(reg, function);

            state.set_register_state(reg, function->underlying_type()->get_pointer_to(), function);
            return bytecode::Operand(reg);
        }
        default:
            return err(span(), "'{}' does not refer to a value", m_path.format());
    }

    return {};
}

BytecodeResult TupleExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    Vector<bytecode::Operand> operands;
    Vector<Type*> types;
    
    for (auto& expr : m_elements) {
        auto operand = TRY(ensure(state, *expr, {}));

        types.push_back(state.type(operand));
        operands.push_back(operand);
    }

    auto* type = TupleType::get(state.context(), types);
    auto reg = select_dst(state, dst);

    state.emit<bytecode::NewTuple>(reg, type, move(operands));
    state.set_register_state(reg, type);

    return bytecode::Operand(reg);
}

BytecodeResult EnumExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult ImportExpr::generate(State& state, Optional<bytecode::Register>) const {
    String qualified_name = m_path.format();

    auto module = state.get_global_module(qualified_name);
    auto current_scope = state.scope();

    auto prev_scope = current_scope;
    if (module) {
        if (module->is_importing()) {
            return err(span(), "Could not import '{}' because a circular dependency was detected", m_path.name());
        }

        current_scope->add_symbol(module);
        return {};
    }

    String fullpath = {};
    // FIXME: ModuleQualifiedName is reversed
    ModuleQualifiedName current_qualified_name;

    for (auto& seg : m_path.segments()) {
        if (seg.has_generic_arguments()) {
            return err(span(), "Generic arguments are not allowed in import paths");
        }

        auto& segment = seg.name();

        fullpath.append(segment);
        fs::Path path(fullpath);
        
        if (!path.exists()) {
            path = state.search_import_paths(fullpath);

            if (path.empty()) {
                return err(span(), "Could not find module '{}'", m_path.name());
            }

            fullpath = fullpath.substr(0, fullpath.size() - segment.size()) + String(path);
        }

        if (!path.is_dir()) {
            return err(span(), "Expected a directory, got a file");
        }

        auto* module = current_scope->resolve<Module>(segment);
        RefPtr<Scope> new_scope = nullptr;

        current_qualified_name.append(segment);
        if (!module) {
            RefPtr<Module> mod = nullptr;
            if (state.has_global_module(current_qualified_name)) {
                mod = state.get_global_module(current_qualified_name);
            } else {
                auto scope = Scope::create(segment, ScopeType::Module);
                mod = Module::create(segment, current_qualified_name, path, scope);

                state.add_global_module(mod);
            }

            new_scope = mod->scope();
            current_scope->add_symbol(mod);
        } else {
            new_scope = module->scope();
        }

        current_scope = new_scope;
        fullpath.push_back('/');
    }

    fs::Path path = fs::Path(fullpath + m_path.name() + FILE_EXTENSION);
    String name = path;

    if (!path.exists()) {
        fs::Path dir = path.with_extension();
        if (!dir.exists()) {
            dir = state.search_import_paths(dir);
            if (dir.empty()) {
                return err(span(), "Could not find module '{}'", m_path.name());
            }
        }

        if (!dir.is_dir()) {
            return err(span(), "Expected a directory, got a file");
        }

        name = dir;
        path = dir.join("module.qr");

        if (!path.exists()) {
            auto scope = Scope::create(m_path.name(), ScopeType::Module);
            auto module = Module::create(m_path.name(), qualified_name, path, scope);

            current_scope->add_symbol(module);
            state.add_global_module(module);

            return {};
        }

        if (!path.is_regular_file()) {
            err(span(), "Expected a file, got a directory");
        }
    }

    auto* prev_module = state.module();
    current_scope = Scope::create(m_path.name(), ScopeType::Module);

    module = Module::create(m_path.name(), qualified_name, path, current_scope);
    prev_scope->add_symbol(module);

    state.add_global_module(module);

    state.set_current_scope(current_scope);
    state.set_current_module(&*module);

    auto source_code = SourceCode::from_path(path);
    Lexer lexer(source_code);

    Vector<Token> tokens = TRY(lexer.lex());

    Parser parser(move(tokens));
    auto ast = TRY(parser.parse());

    for (auto& expr : ast) {
        TRY(expr->generate(state, {}));
    }

    state.set_current_scope(prev_scope);
    state.set_current_module(prev_module);

    if (m_is_wildcard) {
        for (auto& [name, symbol] : current_scope->symbols()) {
            if (symbol->is<Module>() || !symbol->is_public()) {
                continue;
            }

            prev_scope->add_symbol(symbol);
        }
    }

    for (auto& sym : m_symbols) {
        auto* symbol = current_scope->resolve(sym);
        if (!symbol) {
            return err(span(), "Unknown symbol '{}' for '{}'", sym, m_path.format());
        }

        prev_scope->add_symbol(current_scope->symbols().at(sym));   
    }

    module->set_state(Module::Ready);
    return {};
}

BytecodeResult UsingExpr::generate(State& state, Optional<bytecode::Register>) const {
    auto scope = TRY(state.resolve_scope_path(span(), m_path));
    auto* module = scope->resolve<Module>(m_path.name());

    if (!module) {
        return err("Could not find module '{}'", m_path.format());
    }

    scope = module->scope();
    auto current_scope = state.scope();

    for (auto& name : m_symbols) {
        auto* symbol = scope->resolve(name);
        if (!symbol) {
            return err(span(), "Unknown symbol '{}' for '{}'", name, m_path.format());
        }

        current_scope->add_symbol(scope->symbols().at(name));
    }

    return {};
}

BytecodeResult ModuleExpr::generate(State& state, Optional<bytecode::Register>) const {
    auto* prev_module = state.module();
    auto current_scope = state.scope();

    String qualified_name = m_name;
    if (prev_module) {
        qualified_name = format("{}::{}", prev_module->qualified_name(), m_name);
    }

    auto scope = Scope::create(m_name, ScopeType::Module, current_scope);
    auto module = Module::create(m_name, qualified_name, {}, scope);

    current_scope->add_symbol(module);

    state.set_current_scope(scope);
    state.set_current_module(module.get());

    for (auto& expr : m_body) {
        TRY(expr->generate(state, {}));
    }

    state.set_current_scope(current_scope);
    state.set_current_module(prev_module);

    module->set_state(Module::Ready);
    state.add_global_module(module);

    return {};
}

BytecodeResult TernaryExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult ForExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult RangeForExpr::generate(State& state, Optional<bytecode::Register>) const {
    Function* current_function = state.function();
    auto current_scope = state.scope();

    auto* end_block = state.create_block();
    auto* body_block = state.create_block();

    TemporaryChange<Loop> change(current_function->current_loop(), { body_block, end_block });

    bytecode::Operand start = TRY(ensure(state, *m_start, {}));
    Type* type = state.type(start);

    Optional<bytecode::Operand> end;
    if (m_end) {
        end = TRY(ensure(state, *m_end, {}));
        end = TRY(state.type_check_and_cast(m_end->span(), *end, type, "Cannot iterate over a range of different types"));
    }

    auto reg = state.allocate_register();

    size_t local_index = current_function->allocate_local();
    current_function->set_local_type(local_index, type);

    auto variable = Variable::create(m_identifier.value, local_index, type);
    current_scope->add_symbol(variable);

    state.emit<bytecode::SetLocal>(local_index, start);

    state.emit<bytecode::Jump>(body_block);
    current_function->insert_block(body_block);

    state.switch_to(body_block);
    TRY(m_body->generate(state, {}));

    state.set_register_state(reg, type);
    state.emit<bytecode::GetLocal>(reg, local_index);

    state.emit<bytecode::Add>(reg, reg, bytecode::Operand(1, type));
    state.emit<bytecode::SetLocal>(local_index, reg);
    if (m_end) {
        if (m_inclusive) {
            state.emit<bytecode::Lt>(reg, *end, reg);
        } else {
            state.emit<bytecode::Eq>(reg, *end, reg);
        }

        state.emit<bytecode::JumpIf>(reg, end_block, body_block);
    } else {
        state.emit<bytecode::Jump>(body_block);
    }

    current_function->insert_block(end_block);
    state.switch_to(end_block);
    
    return {};
}

BytecodeResult ArrayFillExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult TypeAliasExpr::generate(State& state, Optional<bytecode::Register>) const {
    auto iterator = STR_TO_TYPE.find(m_name);
    if (iterator != STR_TO_TYPE.end()) {
        return err(span(), "Cannot redefine a built-in type");
    }

    Vector<GenericTypeParameter> parameters = TRY(parse_generic_parameters(state, m_parameters));
    bool is_generic = !m_parameters.empty();

    if (is_generic) {
        auto alias = TypeAlias::create(m_name, parameters, &*m_type, m_is_public);
        alias->set_module(state.module());

        state.scope()->add_symbol(alias);
        return {};
    }

    Type* underlying_type = TRY(m_type->evaluate(state));

    auto alias = TypeAlias::create(m_name, underlying_type, m_is_public);
    alias->set_module(state.module());

    state.scope()->add_symbol(alias);
    return {};
}

BytecodeResult StaticAssertExpr::generate(State& state, Optional<bytecode::Register>) const {
    Constant* constant = TRY(state.constant_evaluator().evaluate(*m_condition));
    if (!constant->is<ConstantInt>()) {
        return err(m_condition->span(), "Static assert condition must be a constant boolean expression");
    }

    auto* condition = constant->as<ConstantInt>();
    if (condition->value() != 0) {
        return {};
    }

    if (m_message.empty()) {
        return err(span(), "Static assert failed");
    } else {
        return err(span(), "Static assert failed: {}", m_message);
    }

    return {};
}

BytecodeResult MaybeExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult MatchExpr::generate(State& state, Optional<bytecode::Register>) const {
    auto current_function = state.function();

    bytecode::Operand match = TRY(ensure(state, *m_value, {}));
    Type* type = state.type(match);

    // TODO: Support enums
    if (!type->is_int()) {
        return err(m_value->span(), "Match expressions can only be performed on integer types");
    }

    Vector<bytecode::BasicBlock*> blocks;
    blocks.reserve(m_arms.size());

    bytecode::BasicBlock* default_block = nullptr;

    for (auto& arm : m_arms) {
        auto* block = state.create_block();
        current_function->insert_block(block);

        if (arm.is_wildcard()) {
            default_block = block;
            continue;
        }

        blocks.push_back(block);
    }
    
    bytecode::BasicBlock* end = state.create_block();

    auto generate_pattern_match = [&](
        MatchArm const& arm,
        bytecode::BasicBlock* block,
        bytecode::BasicBlock* next
    ) -> ErrorOr<void> {
        state.switch_to(block);

        auto* body = state.create_block();
        current_function->insert_block(body);

        auto& pattern = arm.pattern;
        if (pattern.is_conditional) {
            auto operand = TRY(ensure(state, *pattern.values[0], {}));
            state.emit<bytecode::JumpIf>(operand, body, next);

            state.switch_to(body);
            TRY(arm.body->generate(state, {}));

            if (!body->is_terminated()) {
                state.emit<bytecode::Jump>(end);
            }

            return {};
        }

        bytecode::Register reg = state.allocate_register();
        if (pattern.values.size() > 1) {
            state.set_register_state(reg, state.context().i1());
            state.emit<bytecode::Move>(reg, 0);

            for (auto& value : pattern.values) {
                Constant* constant = TRY(state.constant_evaluator().evaluate(*value));
                if (!constant->is<ConstantInt>()) {
                    return err(value->span(), "Match patterns must be constant integer expressions");
                }
    
                auto operand = constant->as<ConstantInt>()->to_operand();
    
                bytecode::Register temp = state.allocate_register();
        
                state.emit<bytecode::Eq>(temp, match, operand);
                state.emit<bytecode::Or>(reg, reg, bytecode::Operand(temp));
            }
        } else {
            auto& value = *pattern.values[0];

            Constant* constant = TRY(state.constant_evaluator().evaluate(value));
            if (!constant->is<ConstantInt>()) {
                return err(value.span(), "Match patterns must be constant integer expressions");
            }

            auto operand = constant->as<ConstantInt>()->to_operand();
            state.emit<bytecode::Eq>(reg, match, operand);
        }

        state.emit<bytecode::JumpIf>(bytecode::Operand(reg), body, next);

        state.switch_to(body);
        TRY(arm.body->generate(state, {}));

        if (!body->is_terminated()) {
            state.emit<bytecode::Jump>(end);
        }

        return {};
    };

    auto iterator = blocks.begin();
    state.emit<bytecode::Jump>(*iterator);

    for (auto& arm : m_arms) {
        if (arm.is_wildcard()) {
            state.switch_to(default_block);
            TRY(arm.body->generate(state, {}));

            if (!default_block->is_terminated()) {
                state.emit<bytecode::Jump>(end);
            }

            ++iterator;
            continue;
        }

        auto* next = default_block ? default_block : end;
        if ((iterator + 1) != blocks.end()) {
            next = (*(iterator + 1));
        }

        TRY(generate_pattern_match(arm, *iterator, next));
        ++iterator;
    }

    current_function->insert_block(end);
    state.switch_to(end);

    return {};
}

String extract_name_from_type(ast::TypeExpr const& type) {
    if (type.kind() != TypeKind::Named) {
        return {};
    }

    auto* named = type.as<ast::NamedTypeExpr>();
    auto& path = named->path();

    return path.name();
}

void create_impl_conditions(Vector<OwnPtr<ImplCondition>>& conditions, Set<String>& parameters, ast::TypeExpr& type) {
    switch (type.kind()) {
        case TypeKind::Pointer: {
            // FIXME: Handle double pointers and more
            auto* ptr = type.as<ast::PointerTypeExpr>();
            String name = extract_name_from_type(ptr->pointee());

            if (name.empty() || !parameters.contains(name)) {
                return;
            }

            conditions.push_back(ImplCondition::create(name, ImplCondition::Pointer));
            break;
        }
        case TypeKind::Reference: {
            auto* ref = type.as<ast::ReferenceTypeExpr>();
            String name = extract_name_from_type(ref->type());

            if (name.empty() || !parameters.contains(name)) {
                return;
            }

            conditions.push_back(ImplCondition::create(name, ImplCondition::Reference));
            break;
        }
    }
}

BytecodeResult ImplExpr::generate(State& state, Optional<bytecode::Register>) const {
    auto current_scope = state.scope();
    if (!m_parameters.empty()) {
        auto range = llvm::map_range(m_parameters, [](auto& param) { return param.name; });
        Set<String> parameters(range.begin(), range.end());

        Vector<OwnPtr<ImplCondition>> conditions;
        create_impl_conditions(conditions, parameters, *m_type);

        if (conditions.empty()) {
            return err(span(), "Impl is generic but doesn't use generic parameters");
        }
        
        auto impl = Impl::create(current_scope, &*m_type, &*m_body, move(conditions));
        state.add_impl(move(impl));

        return {};
    }

    Type* underlying_type = TRY(m_type->evaluate(state));
    if (underlying_type->is_struct()) {
        auto* structure = state.get_global_struct(underlying_type);
        auto previous_scope = state.scope();

        state.set_current_scope(structure->scope());
        state.set_current_struct(structure);
        state.set_self_type(underlying_type);

        TRY(m_body->generate(state, {}));

        state.set_current_scope(previous_scope);
        state.set_self_type(nullptr);
        state.set_current_struct(nullptr);

        return {};
    }

    auto scope = Scope::create(underlying_type->str(), ScopeType::Impl, current_scope);

    auto impl = Impl::create(underlying_type, scope);
    state.set_self_type(impl->underlying_type());
    
    state.set_current_scope(scope);
    TRY(m_body->generate(state, {}));

    state.set_current_scope(current_scope);
    state.add_impl(move(impl));

    state.set_self_type(nullptr);
    return {};
}

BytecodeResult TraitExpr::generate(State& state, Optional<bytecode::Register>) const {
    auto current_scope = state.scope();

    auto* type = TraitType::get(state.context(), Symbol::parse_qualified_name(m_name, current_scope));
    auto scope = Scope::create(type->name(), ScopeType::Namespace, current_scope);

    auto trait = Trait::create(m_name, type, move(scope));

    state.set_self_type(type);
    state.add_trait(trait);

    state.set_current_scope(trait->scope());
    for (auto& expr : m_body) {
        if (!expr->is<FunctionDeclExpr>()) {
            auto* function = expr->as<FunctionExpr>();

            TRY(state.type_checker().type_check(*expr));
            trait->add_predefined_function(function);

            continue;
        }

        TRY(state.type_checker().type_check(*expr));
    }

    current_scope->add_symbol(trait);

    state.set_self_type(nullptr);
    state.set_current_scope(current_scope);

    return {};
}

static ErrorOr<void> verify_trait_implementation(Function const* function, Function const* impl) {
    auto ordering = function->parameters().size() <=> impl->parameters().size();
    if (ordering != 0) {
        auto error = err(impl->span(), "Impl function '{}' has {} parameters than the trait function", impl->name(), ordering > 0 ? "fewer" : "more");
        error.add_note(function->span(), "Trait function defined here");

        return error;
    }

    if (function->is_variadic() != impl->is_variadic()) {
        auto error = err(impl->span(), "Impl function '{}' must {}be variadic", impl->name(), function->is_variadic() ? "" : "not ");
        error.add_note(function->span(), "Trait function defined here");

        return error;
    }

    static auto verify_mutability = [](FunctionParameter const& p1, FunctionParameter const& p2) -> ErrorOr<void> {
        if (p1.flags != p2.flags) {
            auto error = err(p2.span, "Parameter '{}' of impl function must have the same mutability as the trait function", p2.name);
            error.add_note(p1.span, "Trait parameter defined here");

            return error;
        }

        return {};
    };

    for (auto [p1, p2] : llvm::zip_equal(function->parameters(), impl->parameters())) {
        if (p1.is_self() && !p2.is_self()) {
            return err(p2.span, "The first parameter of a method must be 'self'");
        } else if (p1.is_self() && p2.is_self()) {
            TRY(verify_mutability(p1, p2));
            continue;
        }

        TRY(verify_mutability(p1, p2));
        if (p1.type != p2.type) {
            auto error = err(p2.span, "Parameter '{}' of impl function '{}' must have the same type as the trait function", p2.name, impl->name());
            error.add_note(p1.span, "Trait parameter defined here");

            return error;
        }
    }

    if (function->return_type() != impl->return_type()) {
        auto error = err(impl->span(), "Return type of impl function '{}' must be the same as the trait function", impl->name());
        error.add_note(function->span(), "Trait function defined here");

        return error;
    }

    return {};
}

BytecodeResult ImplTraitExpr::generate(State& state, Optional<bytecode::Register>) const {
    Type* trait_type = TRY(m_trait->evaluate(state));
    if (!trait_type->is_trait()) {
        return err(m_trait->span(), "Expected a trait type");
    }

    auto trait = state.get_trait(trait_type);
    Type* type = TRY(m_type->evaluate(state));

    if (!type->is_struct()) {
        ASSERT(false, "Only structs can implement traits for now");
    }

    auto current_scope = state.scope();
    auto structure = state.get_global_struct(type);

    state.set_current_scope(structure->scope());
    state.set_self_type(type);
    
    for (auto& expr : m_body) {
        if (!expr->is<FunctionExpr>()) {
            return err(expr->span(), "Only function implementations are allowed in trait impls");
        }

        TRY(expr->generate(state, {}));
        String name = expr->as<FunctionExpr>()->decl().name();

        Function const* function = trait->get_method(name);
        if (!function) {
            return err(expr->span(), "Function '{}' is not part of the trait '{}'", name, trait->name());
        }

        TRY(verify_trait_implementation(function, structure->get_method(name)));
    }

    for (auto& [name, symbol] : trait->scope()->symbols()) {
        if (!symbol->is<Function>()) {
            continue;
        }

        auto* function = symbol->as<Function>();
        if (!function->is_decl()) {
            continue;
        }
        
        auto* impl = structure->get_method(name);
        if (!impl) {
            auto error = err(span(), "Struct '{}' does not implement required function '{}' of trait '{}'", structure->name(), name, trait->name());
            error.add_note(function->span(), "Trait function defined here");
            
            return error;
        }
    }

    for (auto& function : trait->predefined_functions()) {
        TRY(function->generate(state, {}));
    }

    state.set_current_scope(current_scope);
    state.set_self_type(nullptr);

    structure->add_impl_trait(trait->underlying_type());
    return {};
}

BytecodeResult ConstEvalExpr::generate(State& state, Optional<bytecode::Register>) const {
    for (auto& expr : m_body) {
        TRY(state.constant_evaluator().evaluate(*expr));
    }

    return {};
}

}