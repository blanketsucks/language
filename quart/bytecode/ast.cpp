#include <quart/language/state.h>
#include <quart/parser/parser.h>
#include <quart/parser/ast.h>
#include <quart/temporary_change.h>
#include <quart/lexer/lexer.h>
#include <quart/language/trait.h>
#include <quart/stacktrace.h>
#include <quart/casting.h>

#include <unordered_set>
#include <ranges>

namespace quart::ast {

using bytecode::Value, bytecode::Constant;

using bytecode::ConstantFloat, bytecode::ConstantInt, bytecode::ConstantString;
using bytecode::ConstantNull, bytecode::ConstantArray, bytecode::ConstantStruct;

struct ModuleQualifiedName {
    String name;

    explicit ModuleQualifiedName(String name) : name(move(name)) {}
    explicit ModuleQualifiedName() = default;

    operator String() const { return name; }

    void append(String const& segment) {
        if (name.empty()) {
            name.append(segment);
            return;
        }

        name.append("::");
        name.append(segment);
    }
};

static inline ErrorOr<Value*> ensure(State& state, Expr const& expr) {
    auto option = TRY(expr.generate(state));
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

BytecodeResult BlockExpr::generate(State& state) const {
    bool returned = false;
    for (auto& expr : m_block) {
        if (returned) {
            String warning = SourceCode::format_warning(expr->span(), "Unreachable code");
            outln(warning);
        }

        if (isa<ReturnExpr>(expr)) {
            returned = true;
        }

        TRY(expr->generate(state));
    }

    return {};
}

BytecodeResult ExternBlockExpr::generate(State& state) const {
    for (auto& expr : m_block) {
        TRY(expr->generate(state));
    }

    return {};
}

BytecodeResult IntegerExpr::generate(State& state) const {
    IntType* type = nullptr;
    Type* context = state.type_context();

    if (context && context->is_int()) {
        type = cast_unchecked<IntType>(context);
    } else if (m_suffix.type != ast::BuiltinType::None) {
        // We are 100% sure we get an int type from get_type_from_builtin so casting here is ok
        type = cast_unchecked<IntType>(state.get_type_from_builtin(m_suffix.type));
    } else {
        type = state.context().i32();
    }

    return state.context().create_int_constant(m_value, type);
}

BytecodeResult StringExpr::generate(State& state) const {
    return state.context().create_string_constant(m_value, state.context().cstr());
}

BytecodeResult BoolExpr::generate(State& state) const {
    auto& ctx = state.context();
    
    switch (m_value) {
        case BoolExpr::False:
        case BoolExpr::True:
            return ctx.create_int_constant(m_value, ctx.i1());
        case BoolExpr::Null: {
            Type* type = state.type_context();
            if (!type) {
                type = ctx.void_type()->get_pointer_to();
            }

            return state.emit<bytecode::Null>(type);
        }
    }

    ASSERT(false, "Unreachable");
    return {};
}

BytecodeResult ArrayExpr::generate(State& state) const {
    if (m_elements.empty()) {
        return err(span(), "Empty array expressions are not allowed");
    }

    auto& ctx = state.context();
    Vector<Value*> elements;

    quart::Type* array_element_type = nullptr;
    for (auto& expr : m_elements) {
        auto value = TRY(ensure(state, *expr));
        if (elements.empty()) {
            elements.push_back(value);
            array_element_type = value->type();

            continue;
        }
    
        value = TRY(state.type_check_and_cast(expr->span(), value, array_element_type, "Array elements must have the same type"));
        elements.emplace_back(value);
    }

    auto* type = ctx.create_array_type(array_element_type, elements.size());
    return state.emit<bytecode::NewArray>(elements, type);
}

BytecodeResult IdentifierExpr::generate(State& state) const {
    auto* symbol = state.scope()->resolve(m_name);
    if (!symbol) {
        return err(span(), "Unknown identifier '{}'", m_name);
    }

    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = cast_unchecked<Variable>(symbol);
            return variable->emit(state);
        }
        case Symbol::Function: {
            return cast_unchecked<Function>(symbol);
        }
        default:
            return err(span(), "'{}' does not refer to a value", m_name);
    }
}

BytecodeResult FloatExpr::generate(State& state) const {
    Type* type = m_is_double ? state.context().f64() : state.context().f32();
    return ConstantFloat::get(state.context(), type, m_value);
}

static ErrorOr<void> create_global_variable(State& state, String const& name, ast::Expr* value, Type* type, u8 flags) {
    state.set_type_context(type);

    Constant* constant = nullptr;
    if (!value) {
        ASSERT(type, "Type must be specified when an initializer is provided");
        constant = ConstantNull::get(state.context(), type);
    } else {
        constant = TRY(state.constant_evaluator().evaluate(*value));
    }

    size_t global_index = state.allocate_global();

    auto variable = Variable::create(name, global_index, constant->type(), flags);
    variable->set_module(state.module());

    variable->set_initializer(constant);

    state.scope()->add_symbol(variable);
    state.add_global(move(variable));

    state.set_type_context(nullptr);
    return {};
}

BytecodeResult AssignmentExpr::generate(State& state) const {
    Value* value = nullptr;
    Function* current_function = state.function();

    Type* type = m_type ? TRY(m_type->evaluate(state)) : nullptr;
    if (!current_function) {
        u8 flags = Variable::Global;
        if (m_identifier.is_mutable) {
            flags |= Variable::Mutable;
        } if (m_is_public) {
            flags |= Variable::Public;
        }

        TRY(create_global_variable(state, m_identifier.value, m_value.get(), type, flags));
        return {};
    }

    if (m_value) {
        value = TRY(ensure(state, *m_value));
    }

    bool is_struct_value = false;

    if (value) {
        if (!type) {
            type = value->type();
        } else {
            value = TRY(state.type_check_and_cast(span(), value, type, "Cannot assign a value of type '{}' to a variable of type '{}'"));
        }

        // if (value->is_register()) {
        //     auto& register_state = state.register_state(value->reg());
        //     is_struct_value = register_state.flags & RegisterState::Struct;
    
        //     if (is_struct_value) {
        //         type = type->get_pointee_type();
        //     }
        // }

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
    if (is_struct_value) {
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

BytecodeResult TupleAssignmentExpr::generate(State&) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult ConstExpr::generate(State& state) const {
    Type* type = m_type ? TRY(m_type->evaluate(state)) : nullptr;

    u8 flags = Variable::Constant | Variable::Global;
    if (m_is_public) {
        flags |= Variable::Public;
    }

    TRY(create_global_variable(state, m_name, m_value.get(), type, flags));
    return {};
}

BytecodeResult UnaryOpExpr::generate(State& state) const {
    switch (m_op) {
        case UnaryOp::Not: {
            Value* value = TRY(ensure(state, *m_value));
            return state.emit<bytecode::Not>(value);
        }
        case UnaryOp::DeRef: {
            Value* value = TRY(ensure(state, *m_value));
            Type* type = value->type();

            if (!type->is_pointer() && !type->is_reference()) {
                return err(span(), "Cannot de-reference value of type '{}'", type->str());
            }

            return state.emit<bytecode::Read>(value);
        }
        default:
            ASSERT(false, "Unimplemented");
    }

    return {};
}

BytecodeResult BinaryOpExpr::generate(State& state) const {
    if (m_op == BinaryOp::Assign) {
        if (isa<UnaryOpExpr>(m_lhs)) {
            auto* unary = cast_unchecked<UnaryOpExpr>(m_lhs);

            if (unary->op() == UnaryOp::DeRef) {
                auto& value = unary->value();

                auto lhs = TRY(ensure(state, value));
                Type* type = lhs->type();

                if (!type->is_pointer() && !type->is_reference()) {
                    return err(value.span(), "Cannot dereference a value of type '{}'", type->str());
                }

                if (!type->is_mutable()) {
                    return err(value.span(), "Cannot assign to a non-mutable reference");
                }

                auto rhs = TRY(ensure(state, *m_rhs));
                rhs = TRY(state.type_check_and_cast(m_rhs->span(), rhs, type->underlying_type(), "Cannot assign a value of type '{}' to a variable of type '{}'"));

                state.emit<bytecode::Write>(lhs, rhs);
                return {};
            }

            return err(span(), "Invalid left-hand side of assignment");
        }

        auto lhs = TRY(state.resolve_reference(*m_lhs, ReferenceAccess::Mutable));
        auto rhs = TRY(ensure(state, *m_rhs));

        Type* lhs_type = lhs->type()->get_reference_type();
        rhs = TRY(state.type_check_and_cast(m_lhs->span(), rhs, lhs_type, "Cannot assign a value of type '{}' to a variable of type '{}'"));

        state.emit<bytecode::Write>(lhs, rhs);
        return {};
    }

    Value* lhs = TRY(ensure(state, *m_lhs));
    Type* lhs_type = lhs->type();

    state.set_type_context(lhs_type);
    Value* rhs = TRY(ensure(state, *m_rhs));

    rhs = TRY(state.type_check_and_cast(span(), rhs, lhs_type, "Cannot perform binary operation on operands of type '{}' and '{}'"));
    
    Value* result = nullptr;
    switch (m_op) {
        // NOLINTNEXTLINE
        #define Op(x) case BinaryOp::x: result = state.emit<bytecode::x>(lhs, rhs); break;
            ENUMERATE_BINARY_OPS(Op)
        #undef Op

        default:
            return err(span(), "Unknown binary operator");
    }

    if (is_comparison_operator(m_op)) {
        result->set_type(state.context().i1());
    }

    return result;
}

BytecodeResult InplaceBinaryOpExpr::generate(State& state) const {
    auto ref = TRY(state.resolve_reference(*m_lhs, ReferenceAccess::Mutable));
    Type* type = ref->type()->get_reference_type();

    Value* lhs = state.emit<bytecode::Read>(ref);

    auto rhs = TRY(ensure(state, *m_rhs));
    rhs = TRY(state.type_check_and_cast(span(), rhs, type, "Cannot assign a value of type '{}' to a variable of type '{}'"));

    Value* result = nullptr;
    switch (m_op) {
        // NOLINTNEXTLINE
        #define Op(x) case BinaryOp::x: result = state.emit<bytecode::x>(lhs, rhs); break;
            ENUMERATE_BINARY_OPS(Op)
        #undef Op

        default:
            return err(span(), "Unknown binary operator");
    }

    state.emit<bytecode::Write>(ref, result);
    return {};
}

BytecodeResult ReferenceExpr::generate(State& state) const {
    auto access = m_is_mutable ? ReferenceAccess::Mutable : ReferenceAccess::None;
    return TRY(state.resolve_reference(*m_value, access));
}

static ErrorOr<void> generate_generic_function_call(
    State& state,
    Vector<Value*>& arguments,
    FunctionType const* function_type, 
    Vector<OwnPtr<Expr>> const& args,
    size_t index,
    size_t params
) {
    for (auto& arg : args) {
        if (index >= params && function_type->is_var_arg()) {
            auto operand = TRY(ensure(state, *arg));
            arguments[index] = operand;

            continue;
        }

        Type* parameter_type = function_type->get_parameter_at(index);
        state.set_type_context(parameter_type);

        auto operand = TRY(ensure(state, *arg));

        operand = TRY(state.type_check_and_cast(arg->span(), operand, parameter_type, "Cannot pass a value of type '{}' to a parameter that expects '{}'"));
        arguments[index] = operand;

        state.set_type_context(nullptr);
        index++;
    }

    return {};
}

static ErrorOr<Value*> generate_byval_argument(
    State& state, Type* underlying_type, ast::Expr const& arg
) {
    auto result = state.resolve_reference(arg, ReferenceAccess::None);
    Value* argument = nullptr;

    if (result.is_err()) {
        auto operand = TRY(ensure(state, arg));
        Type* type = operand->type();

        if (type != underlying_type) {
            return err(arg.span(), "Cannot pass a value of type '{}' to a parameter that expects '{}'", type->str(), underlying_type->str());
        }

        argument = state.emit<bytecode::Alloca>(underlying_type);
        state.emit<bytecode::Write>(argument, operand);
    } else {
        Value* src = result.value();

        argument = state.emit<bytecode::Alloca>(underlying_type);
        state.emit<bytecode::Memcpy>(argument, src, underlying_type->size());
    }

    return argument;
}

static ErrorOr<void> generate_function_call(
    State& state,
    Vector<Value*>& arguments,
    Function* function,
    FunctionType const* function_type,
    Vector<OwnPtr<Expr>> const& args,
    size_t index,
    size_t params
) {
    size_t i = index;
    std::unordered_set<size_t> call_exprs;

    for (auto& arg : args) {
        if (!isa<CallExpr>(arg)) {
            index++;
            continue;
        }

        if (index >= params && function_type->is_var_arg()) {
            auto operand = TRY(ensure(state, *arg));
            arguments[index] = operand;
        } else {
            FunctionParameter const& parameter = function->parameters()[index];
            state.set_type_context(parameter.type);

            auto operand = TRY(ensure(state, *arg));
            if (state.self()) {
                return err(arg->span(), "Cannot use a struct method as a value");
            }

            operand = TRY(state.type_check_and_cast(arg->span(), operand, parameter.type, "Cannot pass a value of type '{}' to a parameter that expects '{}'"));
            state.set_type_context(nullptr);

            arguments[index] = operand;
        }

        call_exprs.emplace(index);
        index++;
    }

    index = i;
    for (auto& arg : args) {
        if (call_exprs.contains(index)) {
            index++;
            continue;
        }

        if (index >= params && function_type->is_var_arg()) {
            auto operand = TRY(ensure(state, *arg));
            arguments.push_back(operand);

            continue;
        }

        FunctionParameter const& parameter = function->parameters()[index];
        if (!parameter.is_byval()) {
            state.set_type_context(parameter.type);

            auto operand = TRY(ensure(state, *arg));
            if (state.self()) {
                return err(arg->span(), "Cannot use a struct method as a value");
            }

            operand = TRY(state.type_check_and_cast(arg->span(), operand, parameter.type, "Cannot pass a value of type '{}' to a parameter that expects '{}'"));
            arguments[index] = operand;

            state.set_type_context(nullptr);
            index++;

            continue;
        }

        arguments[index] = TRY(generate_byval_argument(state, parameter.type, *arg));
        index++;
    }

    return {};
}

static ErrorOr<Value*> generate_trait_call_argument(
    State& state,
    FunctionParameter const& parameter,
    Expr const& argument
) {
    auto operand = TRY(ensure(state, argument));
    if (state.self()) {
        return err(argument.span(), "Cannot use a struct method as a value");
    }

    bool is_trait_type = parameter.type->is_underlying_type_of(quart::TypeKind::Trait);
    if (is_trait_type) {
        Type* ty = operand->type();
        
        bool match_reference = parameter.type->is_reference() && ty->is_reference();
        bool match_pointer = parameter.type->is_pointer() && ty->is_pointer();
        bool match_mutability = parameter.type->is_mutable() == ty->is_mutable();

        if ((!match_reference && !match_pointer) || !match_mutability) {
            return err(
                argument.span(),
                "Cannot pass value of type '{}' to parameter of type '{}'", 
                ty->str(), 
                parameter.type->str()
            );
        }

        ty = ty->underlying_type();
        auto* trait_type = cast_unchecked<TraitType>(parameter.type->underlying_type());

        if (!isa<StructType>(ty)) { // TODO: Allow non-struct types to implement traits
            return err(argument.span(), "Type '{}' does not implement trait '{}'", ty->str(), trait_type->str());
        }

        auto structure = cast_unchecked<StructType>(ty)->decl();
        if (!structure->impls_trait(trait_type)) {
            return err(argument.span(), "Type '{}' does not implement trait '{}'", ty->str(), trait_type->str());
        }

        return operand;
    }

    return TRY(state.type_check_and_cast(argument.span(), operand, parameter.type, "Cannot pass a value of type '{}' to a parameter that expects '{}'"));
}

static ErrorOr<RefPtr<Function>> generate_trait_function_call(
    State& state,
    Vector<Value*>& arguments,
    Function* function,
    FunctionType const* function_type,
    Vector<OwnPtr<Expr>> const& args,
    size_t index,
    size_t params
) {
    Vector<FunctionParameter> parameters;
    for (auto& arg : args) {
        if (index >= params && function_type->is_var_arg()) {
            auto operand = TRY(ensure(state, *arg));
            arguments.push_back(operand);

            continue;
        }

        FunctionParameter const& parameter = function->parameters()[index];
        if (!parameter.is_byval()) {
            state.set_type_context(parameter.type);

            auto operand = TRY(generate_trait_call_argument(state, parameter, *arg));

            arguments[index] = operand;
            parameters.push_back(parameter.clone(operand->type()));

            state.set_type_context(nullptr);
            index++;

            continue;
        }

        arguments[index] = TRY(generate_byval_argument(state, parameter.type, *arg));
        parameters.push_back(parameter.clone(parameter.type->get_pointer_to()));

        index++;
    }

    return TRY(function->specialize(state, parameters));
}

BytecodeResult CallExpr::generate(State& state) const {
    Value* callee = TRY(ensure(state, *m_callee));

    Type* type = callee->type();
    Function* function = cast<Function>(callee);

    FunctionType const* function_type = nullptr;
    if (type->is_pointer()) {
        Type* pointee = type->get_pointee_type();
        if (!pointee->is_function()) {
            return err(span(), "Cannot call a value of type '{}'", type->str());
        }

        function_type = cast_unchecked<FunctionType>(pointee);
    } else if (type->is_function()) {
        function_type = cast_unchecked<FunctionType>(type);
    } else {
        return err(span(), "Cannot call a value of type '{}'", type->str());
    }

    Optional<Value*> self = state.self();

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

    Vector<Value*> arguments;
    arguments.resize(m_args.size() + self.has_value());

    if (self.has_value()) {
        arguments[0] = self.value();
        state.reset_self();
    }

    Optional<Value*> struct_return = {};
    if (function) {
        if (function->has_trait_parameter()) {
            auto specialized = TRY(generate_trait_function_call(state, arguments, function, function_type, m_args, index, params));
            return state.emit<bytecode::Call>(specialized.get(), specialized->underlying_type(), arguments);
        }

        if (function->is_struct_return()) {
            struct_return = state.emit<bytecode::Alloca>(function->return_type());
            arguments.emplace_back(*struct_return);
        }

        TRY(generate_function_call(state, arguments, function, function_type, m_args, index, params));
    } else {
        TRY(generate_generic_function_call(state, arguments, function_type, m_args, index, params));
    }

    Value* value = state.emit<bytecode::Call>(callee, function_type, arguments);
    if (struct_return.has_value()) {
        return struct_return.value();
    } else {
        return value;
    }
}

static ErrorOr<void> generate_struct_return(State& state, Function* function, ast::Expr const& value) {
    auto result = state.resolve_reference(value, ReferenceAccess::None);
    Value* return_value = nullptr;

    Type* return_type = function->return_type();
    if (result.is_err()) {
        return_value = TRY(ensure(state, value));
        Type* type = return_value->type();

        // TODO: Handle more sophisticated cases
        if (!isa<bytecode::Alloca>(return_value)) {
            return err(value.span(), "Cannot return a value of type '{}' from a function that expects '{}'", type->str(), return_type->str());
        }
    } else {
        return_value = result.value();
        Type* type = return_value->type()->get_reference_type();

        if (type != return_type) {
            return err(value.span(), "Cannot return a value of type '{}' from a function that expects '{}'", type->str(), return_type->str());
        }
    }

    Value* struct_return = state.emit<bytecode::GetLocal>(return_type->get_pointer_to(), 0);
    state.emit<bytecode::Memcpy>(struct_return, return_value, return_type->size());

    return {};
}

BytecodeResult ReturnExpr::generate(State& state) const {
    Function* current_function = state.function();
    auto* previous_block = state.current_block();

    bytecode::BasicBlock* return_block = current_function->return_block();
    if (current_function->has_defers()) {
        if (current_function->new_defer_block_needed()) {
            return_block = state.create_block();
            state.switch_to(return_block);

            current_function->set_return_block(return_block);
            current_function->set_new_defer_block_needed(false);

            for (auto& defer : current_function->defers()) {
                TRY(defer->generate(state));
            }

            state.switch_to(previous_block);
            current_function->emit_return_block_body(state);
        }
    }

    Type* return_type = current_function->return_type();
    if (current_function->is_struct_return()) {
        if (!m_value) {
            return err(span(), "Cannot return void from a function that expects '{}'", return_type->str());
        }

        TRY(generate_struct_return(state, current_function, *m_value));
        if (!current_function->has_defers()) {
            state.emit<bytecode::Return>();
            return {};
        }

        state.emit<bytecode::Jump>(return_block);
        return {};
    }

    if (m_value) {
        if (return_type->is_void()) {
            return err(m_value->span(), "Cannot return a value from a function that expects void");
        }

        auto return_register = *state.return_register();

        auto operand = TRY(ensure(state, *m_value));
        operand = TRY(state.type_check_and_cast(m_value->span(), operand, return_type, "Cannot return a value of type '{}' from a function that expects '{}'"));
        
        if (!current_function->has_defers()) {
            state.emit<bytecode::Return>(operand);
            return {};
        }

        state.emit<bytecode::Write>(return_register, operand);
        state.emit<bytecode::Jump>(return_block);
    } else {
        if (!return_type->is_void()) {
            return err(span(), "Cannot return void from a function that expects '{}'", return_type->str());
        }

        if (!current_function->has_defers()) {
            state.emit<bytecode::Return>();
            return {};
        }

        state.emit<bytecode::Jump>(return_block);
    }

    return {};
}

BytecodeResult FunctionDeclExpr::generate(State& state) const {
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
            // type = type->get_pointer_to();
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

BytecodeResult FunctionExpr::generate(State& state) const {
    TRY(m_decl->generate(state));
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

    // FIXME: URGENT
    if (function->is_struct_return()) {
        ASSERT(false, "Unimplemented");
        // auto return_register = state.allocate_register();
        // state.emit<bytecode::GetReturn>(return_register);

        // state.set_register_state(return_register, function->return_type()->get_pointer_to());
        // state.inject_return(return_register);
    } else if (!function->return_type()->is_void()) {
        Value* ret = state.emit<bytecode::Alloca>(function->return_type());
        state.inject_return(ret);
    }
    
    auto* return_block = state.create_block();

    function->set_return_block(return_block);
    function->emit_return_block_body(state);

    TRY(m_body->generate(state));
    TRY(function->finalize_body(state));

    function->insert_return_block();

    state.switch_to(previous_block);
    state.set_current_scope(previous_scope);

    state.set_current_function(previous_function);
    state.reset_return();

    return {};
}

BytecodeResult DeferExpr::generate(State& state) const {
    Function* current_function = state.function();

    TRY(state.type_checker().type_check(*m_expr));

    current_function->add_defer(m_expr.get());
    current_function->set_new_defer_block_needed(true);

    return {};
}

BytecodeResult IfExpr::generate(State& state) const {
    Function* current_function = state.function();
    if (!current_function) {
        return err(span(), "If statements are not allowed outside functions");
    }

    auto* then_block = state.create_block();
    auto* else_block = state.create_block();

    auto operand = TRY(ensure(state, *m_condition));
    operand = TRY(state.type_check_and_cast(m_condition->span(), operand, state.context().i1(), "If conditions must be booleans"));

    state.emit<bytecode::JumpIf>(operand, then_block, else_block);

    current_function->insert_block(then_block);
    current_function->insert_block(else_block);

    state.switch_to(then_block);
    TRY(m_body->generate(state));

    if (m_else_body) {
        bytecode::BasicBlock* end_block = nullptr;
        if (!then_block->is_terminated()) {
            end_block = state.create_block();
            state.emit<bytecode::Jump>(end_block);
        }

        state.switch_to(else_block);
        TRY(m_else_body->generate(state));

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

BytecodeResult WhileExpr::generate(State& state) const {
    Function* current_function = state.function();
    if (!current_function) {
        return err(span(), "While loops are not allowed outside functions");
    }

    auto operand = TRY(ensure(state, *m_condition));
    operand = TRY(state.type_check_and_cast(m_condition->span(), operand, state.context().i1(), "While conditions must be booleans"));

    auto* while_block = state.create_block();
    auto* end_block = state.create_block();

    TemporaryChange<Loop> change(current_function->current_loop(), { while_block, end_block });

    state.emit<bytecode::JumpIf>(operand, while_block, end_block);
    current_function->insert_block(while_block);

    state.switch_to(while_block);
    TRY(m_body->generate(state));

    operand = TRY(ensure(state, *m_condition));
    state.emit<bytecode::JumpIf>(operand, while_block, end_block);

    current_function->insert_block(end_block);
    state.switch_to(end_block);

    return {};
}

BytecodeResult BreakExpr::generate(State& state) const {
    Function* current_function = state.function();
    auto& current_loop = current_function->current_loop();

    state.emit<bytecode::Jump>(current_loop.end);
    return {};
}

BytecodeResult ContinueExpr::generate(State& state) const {
    Function* current_function = state.function();
    auto& current_loop = current_function->current_loop();

    state.emit<bytecode::Jump>(current_loop.start);
    return {};
}

static ErrorOr<void> generate_generic_struct(State& state, StructExpr const& expr) {
    Vector<GenericTypeParameter> generic_parameters;
    Set<String> names;

    auto scope = Scope::create(expr.name(), ScopeType::Struct, state.scope());

    for (auto& parameter : expr.parameters()) {
        generic_parameters.emplace_back(parameter.name, parameter.span);
        names.insert(parameter.name);
    
        scope->add_symbol(
            TypeAlias::create(
                parameter.name,
                EmptyType::get(state.context(), parameter.name),
                false
            )
        );
    }

    auto* type = StructType::get(
        state.context(),
        Symbol::parse_qualified_name(expr.name(), state.scope()),
        {}
    );

    HashMap<String, quart::StructField> fields;
    Vector<Type*> types;

    auto structure = Struct::create(expr.name(), type, scope, generic_parameters, expr.is_public());
    structure->set_module(state.module());

    for (auto& field : expr.fields()) {
        Type* type = nullptr;
        if (isa<NamedTypeExpr>(field.type)) {
            auto& path = cast_unchecked<NamedTypeExpr>(field.type)->path();
            if (!path.has_segments() && names.contains(path.name())) {
                type = EmptyType::get(state.context(), path.name());
            }
        }

        if (!type) {
            type = TRY(field.type->evaluate(state));
        }

        fields.insert_or_assign(field.name, quart::StructField { field.name, type, field.flags, field.index });
        types.push_back(type);
    }

    type->set_fields(types);
    type->set_decl(structure.get());

    structure->set_fields(move(fields));

    auto previous_scope = state.scope();
    previous_scope->add_symbol(structure);

    state.set_current_scope(scope);
    state.set_current_struct(structure.get());

    state.set_self_type(structure->underlying_type());

    Vector<ast::Expr*> body;
    for (auto& expr : expr.members()) {
        TRY(state.type_checker().type_check(*expr));
        body.push_back(expr.get());
    }

    structure->set_body(move(body));

    state.set_current_scope(previous_scope);
    state.set_self_type(nullptr);
    state.set_current_struct(nullptr);

    return {};
}


BytecodeResult StructExpr::generate(State& state) const {
    if (m_opaque) {
        auto* type = StructType::get(state.context(), Symbol::parse_qualified_name(m_name, state.scope()), {});
        auto structure = Struct::create(m_name, type, state.scope(), m_is_public);

        state.scope()->add_symbol(structure);
        state.emit<bytecode::NewStruct>(&*structure);

        structure->set_module(state.module());
        return {};
    }

    if (!m_parameters.empty()) {
        TRY(generate_generic_struct(state, *this));
        return {};
    }

    auto* type = StructType::get(state.context(), Symbol::parse_qualified_name(m_name, state.scope()), {});
    auto scope = Scope::create(m_name, ScopeType::Struct, state.scope());

    auto structure = Struct::create(m_name, type, {}, scope, m_is_public);
    structure->set_module(state.module());

    type->set_decl(structure.get());
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

    state.set_self_type(structure->underlying_type());

    state.emit<bytecode::NewStruct>(structure.get());
    for (auto& expr : m_members) {
        TRY(expr->generate(state));
    }

    state.set_current_scope(previous_scope);
    state.set_self_type(nullptr);
    state.set_current_struct(nullptr);
    
    return {};
}

BytecodeResult ConstructorExpr::generate(State& state) const {
    Struct* structure = TRY(state.resolve_struct(*m_parent));
    auto& fields = structure->fields();

    Vector<Pair<size_t, Value*>> arguments;
    arguments.reserve(fields.size());

    for (auto& argument : m_arguments) {
        auto iterator = fields.find(argument.name);
        if (iterator == fields.end()) {
            return err(argument.span, "Unknown field '{}' for struct '{}'", argument.name, structure->name());
        }

        auto& field = iterator->second;
        state.set_type_context(field.type);

        auto value = TRY(ensure(state, *argument.value));

        value = TRY(state.type_check_and_cast(argument.value->span(), value, field.type, "Cannot assign a value of type '{}' to a field of type '{}'"));
        arguments.emplace_back(field.index, value);

        state.set_type_context(nullptr);
    }

    Value* struct_alloca = state.emit<bytecode::Alloca>(structure->underlying_type());
    for (auto& [index, argument] : arguments) {
        Constant* idx = ConstantInt::get(state.context(), state.i32(), index);
        state.emit<bytecode::SetMember>(struct_alloca, idx, argument);
    }

    return struct_alloca;
}

BytecodeResult AttributeExpr::generate(State& state) const {
    return TRY(state.generate_attribute_access(*this, ReferenceAccess::None));
}

BytecodeResult IndexExpr::generate(State& state) const {
    return TRY(state.generate_index_access(*this, ReferenceAccess::None));
}

BytecodeResult CastExpr::generate(State& state) const {
    auto value = TRY(ensure(state, *m_value));
    Type* type = TRY(m_to->evaluate(state));

    // FIXME: More checks are needed to be put in place and we can't really use State::type_check_and_cast here
    // because it does a "safe" cast and this is more of a "force" cast.

    // Type* from = value->type();
    // if (type->is_mutable() && !from->is_mutable()) {
    //     return err(span(), "Cannot cast a non-mutable value to a mutable value");
    // }

    return state.emit<bytecode::Cast>(value, type);
}

BytecodeResult SizeofExpr::generate(State& state) const {
    size_t size = TRY(state.size_of(*m_value));
    Type* context = state.type_context();

    Type* type = nullptr;
    if (context && context->is_int()) {
        type = cast_unchecked<IntType>(context);
    } else {
        type = state.context().u32();
    }

    return ConstantInt::get(state.context(), type, size);
}

BytecodeResult OffsetofExpr::generate(State&) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult PathExpr::generate(State& state) const {
    auto scope = TRY(state.resolve_scope_path(span(), m_path));
    auto* symbol = scope->resolve(m_path.name());

    if (!symbol) {
        return err(span(), "Unknown identifier '{}'", m_path.format());
    }

    if (!symbol->is_public() && symbol->module() != state.module()) {
        return err(span(), "Cannot access private symbol '{}'", m_path.format());
    }

    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = cast_unchecked<Variable>(symbol);
            return variable->emit(state);
        }
        case Symbol::Function: {
            return cast_unchecked<Function>(symbol);
        }
        default:
            return err(span(), "'{}' does not refer to a value", m_path.format());
    }

    return {};
}

BytecodeResult TupleExpr::generate(State& state) const {
    Vector<Value*> operands;
    Vector<Type*> types;
    
    for (auto& expr : m_elements) {
        auto operand = TRY(ensure(state, *expr));

        types.push_back(operand->type());
        operands.push_back(operand);
    }

    auto* type = TupleType::get(state.context(), types);
    return state.emit<bytecode::NewTuple>(type, move(operands));
}

BytecodeResult EnumExpr::generate(State&) const {
    return {};
}

BytecodeResult ImportExpr::generate(State& state) const {
    String qualified_name = m_path.format();

    auto module = state.get_global_module(qualified_name);

    auto prev_scope = state.scope();
    auto prev_module = state.module();

    if (module) {
        if (module->is_importing()) {
            return err(span(), "Could not import '{}' because a circular dependency was detected", m_path.name());
        }

        prev_scope->add_symbol(module);
        return {};
    }

    String fullpath = {};
    // FIXME: ModuleQualifiedName is reversed
    ModuleQualifiedName current_qualified_name;

    auto current_scope = prev_scope;
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

    auto scope = Scope::create(m_path.name(), ScopeType::Module);

    module = Module::create(m_path.name(), qualified_name, path, scope);
    prev_scope->add_symbol(module);

    state.add_global_module(module);

    state.set_current_scope(scope);
    state.set_current_module(&*module);

    auto source_code = SourceCode::from_path(path);
    Lexer lexer(source_code);

    Vector<Token> tokens = TRY(lexer.lex());

    Parser parser(move(tokens));
    auto ast = TRY(parser.parse());

    for (auto& expr : ast) {
        TRY(expr->generate(state));
    }

    state.set_current_scope(prev_scope);
    state.set_current_module(prev_module);

    if (m_is_wildcard) {
        for (auto& [name, symbol] : current_scope->symbols()) {
            if (isa<Module>(symbol) || !symbol->is_public()) {
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

BytecodeResult UsingExpr::generate(State& state) const {
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

BytecodeResult ModuleExpr::generate(State& state) const {
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
        TRY(expr->generate(state));
    }

    state.set_current_scope(current_scope);
    state.set_current_module(prev_module);

    module->set_state(Module::Ready);
    state.add_global_module(module);

    return {};
}

BytecodeResult TernaryExpr::generate(State&) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult ForExpr::generate(State&) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult RangeForExpr::generate(State& state) const {
    Function* current_function = state.function();
    auto current_scope = state.scope();

    auto* end_block = state.create_block();
    auto* body_block = state.create_block();

    TemporaryChange<Loop> change(current_function->current_loop(), { body_block, end_block });

    Value* start = TRY(ensure(state, *m_start));
    Type* type = start->type();

    Optional<Value*> end;
    if (m_end) {
        end = TRY(ensure(state, *m_end));
        end = TRY(state.type_check_and_cast(m_end->span(), *end, type, "Cannot iterate over a range of different types"));
    }

    size_t local_index = current_function->allocate_local();
    current_function->set_local_type(local_index, type);

    auto variable = Variable::create(m_identifier.value, local_index, type);
    current_scope->add_symbol(variable);

    state.emit<bytecode::SetLocal>(local_index, start);

    state.emit<bytecode::Jump>(body_block);
    current_function->insert_block(body_block);

    state.switch_to(body_block);
    TRY(m_body->generate(state));

    // state.set_register_state(reg, type);
    // state.emit<bytecode::GetLocal>(reg, local_index);

    // state.emit<bytecode::Add>(reg, reg, Value*(1, type));
    // state.emit<bytecode::SetLocal>(local_index, reg);
    // if (m_end) {
    //     if (m_inclusive) {
    //         state.emit<bytecode::Lt>(reg, *end, reg);
    //     } else {
    //         state.emit<bytecode::Eq>(reg, *end, reg);
    //     }

    //     state.emit<bytecode::JumpIf>(reg, end_block, body_block);
    // } else {
    //     state.emit<bytecode::Jump>(body_block);
    // }

    current_function->insert_block(end_block);
    state.switch_to(end_block);
    
    return {};
}

BytecodeResult ArrayFillExpr::generate(State&) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult TypeAliasExpr::generate(State& state) const {
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

BytecodeResult StaticAssertExpr::generate(State& state) const {
    Constant* constant = TRY(state.constant_evaluator().evaluate(*m_condition));
    if (!isa<ConstantInt>(constant)) {
        return err(m_condition->span(), "Static assert condition must be a constant boolean expression");
    }

    auto* condition = cast_unchecked<ConstantInt>(constant);
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

BytecodeResult MaybeExpr::generate(State&) const {
    ASSERT(false, "Not implemented");
    return {};
}

BytecodeResult MatchExpr::generate(State& state) const {
    auto current_function = state.function();

    Value* match = TRY(ensure(state, *m_value));
    Type* type = match->type();

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
            auto operand = TRY(ensure(state, *pattern.values[0]));
            state.emit<bytecode::JumpIf>(operand, body, next);

            state.switch_to(body);
            TRY(arm.body->generate(state));

            if (!body->is_terminated()) {
                state.emit<bytecode::Jump>(end);
            }

            return {};
        }

        Value* condition = ConstantInt::get(state.context(), state.i1(), 0);
        if (pattern.values.size() > 1) {
            for (auto& value : pattern.values) {
                Constant* constant = TRY(state.constant_evaluator().evaluate(*value));
                if (!isa<ConstantInt>(constant)) {
                    return err(value->span(), "Match patterns must be constant integer expressions");
                }
    
                auto* integer = cast_unchecked<ConstantInt>(constant);
        
                Value* eq = state.emit<bytecode::Eq>(match, integer);
                condition = state.emit<bytecode::Or>(condition, eq);
            }
        } else {
            auto& value = *pattern.values[0];

            Constant* constant = TRY(state.constant_evaluator().evaluate(value));
            if (!isa<ConstantInt>(constant)) {
                return err(value.span(), "Match patterns must be constant integer expressions");
            }

            auto* integer = cast_unchecked<ConstantInt>(constant);
            condition = state.emit<bytecode::Eq>(match, integer);
        }

        state.emit<bytecode::JumpIf>(condition, body, next);

        state.switch_to(body);
        TRY(arm.body->generate(state));

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
            TRY(arm.body->generate(state));

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

BytecodeResult ImplExpr::generate(State& state) const {
    auto current_scope = state.scope();
    if (!m_parameters.empty()) {
        auto scope = Scope::create({}, ScopeType::Impl, current_scope);

        auto range = llvm::map_range(m_parameters, [](auto& param) { return param.name; });
        Set<String> parameters = { range.begin(), range.end() };

        for (auto& parameter : parameters) {
            scope->add_symbol(TypeAlias::create(
                parameter,
                EmptyType::get(state.context(), parameter),
                false
            ));
        }
        
        auto previous_scope = state.scope();
        state.set_current_scope(scope);
        
        Type* type = TRY(m_type->evaluate(state));
        if (type->is_underlying_type_of(quart::TypeKind::Function)) {
            type = type->underlying_type();
        }

        state.set_self_type(type);

        TRY(state.type_checker().type_check(*m_body));

        state.set_current_scope(previous_scope);
        state.set_self_type(nullptr);

        auto impl = Impl::create(type, scope, m_body.get(), parameters);
        state.add_impl(move(impl));

        return {};
    }

    Type* underlying_type = TRY(m_type->evaluate(state));
    if (underlying_type->is_struct()) {
        auto* structure = cast_unchecked<StructType>(underlying_type)->decl();
        auto previous_scope = state.scope();

        state.set_current_scope(structure->scope());
        state.set_current_struct(structure);
        state.set_self_type(underlying_type);

        TRY(m_body->generate(state));

        state.set_current_scope(previous_scope);
        state.set_self_type(nullptr);
        state.set_current_struct(nullptr);

        return {};
    }

    auto scope = Scope::create(underlying_type->str(), ScopeType::Impl, current_scope);

    auto impl = Impl::create(underlying_type, scope);
    state.set_self_type(impl->underlying_type());
    
    state.set_current_scope(scope);
    TRY(m_body->generate(state));

    state.set_current_scope(current_scope);
    state.add_impl(move(impl));

    state.set_self_type(nullptr);
    return {};
}

BytecodeResult TraitExpr::generate(State& state) const {
    auto current_scope = state.scope();

    auto* type = TraitType::get(state.context(), Symbol::parse_qualified_name(m_name, current_scope));
    auto scope = Scope::create(type->name(), ScopeType::Namespace, current_scope);
    
    auto trait = Trait::create(m_name, type, scope);
    for (auto& parameter : m_parameters) {
        auto alias = TypeAlias::create(
            parameter.name,
            EmptyType::get(state.context(), parameter.name), 
            true
        );

        scope->add_symbol(move(alias));
        trait->add_generic_parameter(parameter.name, parameter.span);
    }

    state.set_self_type(type);
    state.add_trait(trait);

    state.set_current_scope(trait->scope());
    for (auto& expr : m_body) {
        trait->add_body_expr(expr.get());
    
        if (!isa<FunctionDeclExpr>(expr)) {
            auto* function = cast_unchecked<FunctionExpr>(expr);

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

static ErrorOr<void> verify_trait_implementation(
    Function const* function,
    Function const* impl
) {
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
        if (p1.is_mutable() != p2.is_mutable()) {
            auto error = err(p2.span, "Parameter '{}' of impl function must have the same mutability as the trait function", p2.name);
            error.add_note(p1.span, "Trait parameter defined here");

            return error;
        }

        return {};
    };

    for (auto [p1, p2] : std::views::zip(function->parameters(), impl->parameters())) {
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

BytecodeResult ImplTraitExpr::generate(State& state) const {
    Type* trait_type = TRY(m_trait->evaluate(state));
    if (!trait_type->is_trait()) {
        return err(m_trait->span(), "Expected a trait type");
    }

    auto trait = state.get_trait(trait_type);
    Type* type = TRY(m_type->evaluate(state));

    RefPtr<Scope> scope = trait->resolve_scope(cast_unchecked<TraitType>(trait_type));
    if (!type->is_struct()) {
        ASSERT(false, "Only structs can implement traits for now");
    }

    auto structure = cast<StructType>(type)->decl();

    auto current_scope = state.scope();

    state.set_current_scope(structure->scope());
    state.set_self_type(type);
    
    for (auto& expr : m_body) {
        if (!isa<FunctionExpr>(expr)) {
            return err(expr->span(), "Only function implementations are allowed in trait impls");
        }

        TRY(expr->generate(state));
        String name = cast_unchecked<FunctionExpr>(expr)->decl().name();

        Function const* function = scope->resolve<Function>(name);
        if (!function) {
            return err(expr->span(), "Function '{}' is not part of the trait '{}'", name, trait->name());
        }

        auto result = verify_trait_implementation(function, structure->get_method(name));
        if (result.is_err() && trait->has_generic_parameters()) {
            auto error = result.error();

            Vector<String> generic_arguments;
            for (auto& [name, span] : trait->generic_parameters()) {
                auto* type_alias = scope->resolve<TypeAlias>(name);
                generic_arguments.push_back(format("{}='{}'", name, type_alias->underlying_type()->str()));
            }

            error.add_note(
                m_trait->span(),
                format("Trait '{}' instantiated here with generic arguments: {}", trait->name(), format_range(generic_arguments))
            );

            return error;
        }
    }

    for (auto& [name, symbol] : trait->scope()->symbols()) {
        if (!isa<Function>(*symbol)) {
            continue;
        }

        auto* function = cast_unchecked<Function>(*symbol);
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

    if (trait->has_generic_parameters()) {
        auto& parameters = trait->generic_parameters();

        auto target = structure->scope();
        for (auto& [name, span] : parameters) {
            target->add_symbol(scope->symbols().at(name));
        }
    }

    for (auto& function : trait->predefined_functions()) {
        TRY(function->generate(state));
    }

    if (trait->has_generic_parameters()) {
        auto& parameters = trait->generic_parameters();
        auto scope = structure->scope();

        for (auto& [name, span] : parameters) {
            scope->remove_symbol(name);
        }
    }

    state.set_current_scope(current_scope);
    state.set_self_type(nullptr);

    structure->add_impl_trait(cast_unchecked<TraitType>(trait_type));
    return {};
}

BytecodeResult ConstEvalExpr::generate(State& state) const {
    return {};
}

}