#include <quart/language/state.h>
#include <quart/parser/ast.h>
#include <quart/temporary_change.h>

namespace quart::ast {

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

BytecodeResult BlockExpr::generate(State& state, Optional<bytecode::Register>) const {
    for (auto& expr : m_block) {
        TRY(expr->generate(state, {}));
    }

    return {};
}

BytecodeResult ExternBlockExpr::generate(State&, Optional<bytecode::Register>) const {
    return {};
}

BytecodeResult IntegerExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto* type = state.types().create_int_type(m_width, true);
    auto op = bytecode::Operand(m_value, type);
    if (!dst.has_value()) {
        return op;
    }

    state.emit<bytecode::Move>(*dst, op);
    state.set_register_type(*dst, type);

    return bytecode::Operand(*dst);
}

BytecodeResult StringExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto reg = select_dst(state, dst);
    state.emit<bytecode::NewString>(reg, m_value);

    state.set_register_type(reg, state.types().cstr());
    return bytecode::Operand(reg);
}

BytecodeResult ArrayExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    if (m_elements.empty()) {
        return err(span(), "Empty array expressions are not allowed");
    }

    auto& registry = state.types();

    auto reg = select_dst(state, dst);
    Vector<bytecode::Operand> ops;

    quart::Type* array_element_type = nullptr;
    for (auto& expr : m_elements) {
        auto operand = TRY(ensure(state, *expr, {}));
        if (ops.empty()) {
            ops.push_back(operand);
            array_element_type = state.type(operand);

            continue;
        }
    
        operand = TRY(state.type_check_and_cast(expr->span(), operand, array_element_type, "Array elements must have the same type"));
        ops.push_back(operand);
    }

    state.emit<bytecode::NewArray>(reg, ops);
    auto* type = registry.create_array_type(array_element_type, ops.size());

    state.set_register_type(reg, type);
    return bytecode::Operand(reg);
}

BytecodeResult IdentifierExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto* symbol = state.scope()->resolve(m_name);
    if (!symbol) {
        return err(span(), "Unknown identifier '{0}'", m_name);
    }

    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = symbol->as<Variable>();
            auto reg = select_dst(state, dst);

            state.emit<bytecode::GetLocal>(reg, variable->local_index());
            state.set_register_type(reg, variable->value_type());

            return bytecode::Operand(reg);
        }
        case Symbol::Function: {
            auto* function = symbol->as<Function>();
            auto reg = select_dst(state, dst);

            state.emit<bytecode::GetFunction>(reg, function);
            state.set_register_type(reg, function->underlying_type()->get_pointer_to());

            return bytecode::Operand(reg);
        }
        default:
            return err(span(), "'{0}' does not refer to a value", m_name);
    }
}

BytecodeResult FloatExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult AssignmentExpr::generate(State& state, Optional<bytecode::Register>) const {
    bytecode::Operand value;
    Function* current_function = state.function();

    if (m_value) {
        value = TRY(ensure(state, *m_value, {}));
    }

    Type* type = m_type ? TRY(m_type->evaluate(state)) : nullptr;
    if (!value.is_none() && type) {
        value = TRY(state.type_check_and_cast(span(), value, type, "Cannot assign a value of type '{0}' to a variable of type '{1}'"));
    } else if (!value.is_none()) {
        type = state.type(value);
    }

    size_t local_index = current_function->allocate_local();
    current_function->set_local_type(local_index, type);

    u8 flags = Variable::None;
    if (m_identifier.is_mutable) {
        flags |= Variable::Mutable;
    }

    auto variable = Variable::create(m_identifier.value, local_index, type, flags);
    state.emit<bytecode::SetLocal>(local_index, value);

    state.scope()->add_symbol(variable);
    return {};
}

BytecodeResult TupleAssignmentExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult ConstExpr::generate(State& state, Optional<bytecode::Register>) const {
    bytecode::Operand value = TRY(ensure(state, *m_value, {}));

    size_t global_index = state.allocate_global();
    auto variable = Variable::create(m_name, global_index, state.type(value));

    state.emit<bytecode::SetGlobal>(global_index, value);
    state.scope()->add_symbol(variable);

    return {};
}

BytecodeResult UnaryOpExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    switch (m_op) {
        case UnaryOp::Not:
            break;
    }

    return {};
}

BytecodeResult BinaryOpExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    if (m_op == BinaryOp::Assign) {
        auto lhs = TRY(state.resolve_reference(*m_lhs, true));
        auto rhs = TRY(ensure(state, *m_rhs, {}));

        Type* lhs_type = state.type(lhs)->get_reference_type();
        rhs = TRY(state.type_check_and_cast(span(), rhs, lhs_type, "Cannot assign a value of type '{0}' to a variable of type '{1}'"));

        state.emit<bytecode::Write>(lhs, rhs);
        return {};
    }

    bytecode::Operand lhs = TRY(ensure(state, *m_lhs, {}));
    bytecode::Operand rhs = TRY(ensure(state, *m_rhs, {}));

    Type* lhs_type = state.type(lhs);
    rhs = TRY(state.type_check_and_cast(span(), rhs, lhs_type, "Cannot perform binary operation on operands of type '{0}' and '{1}'"));

    auto reg = select_dst(state, dst);
    switch (m_op) {
        // NOLINTNEXTLINE
        #define Op(x) case BinaryOp::x: state.emit<bytecode::x>(reg, lhs, rhs); break;
            ENUMERATE_BINARY_OPS(Op)
        #undef Op

        default:
            return err(span(), "Unknown binary operator");
    }

    state.set_register_type(reg, lhs_type);
    return bytecode::Operand(reg);
}

BytecodeResult InplaceBinaryOpExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult ReferenceExpr::generate(State& state, Optional<bytecode::Register>) const {
    auto reg = TRY(state.resolve_reference(*m_value, m_is_mutable));
    return bytecode::Operand(reg);
}

BytecodeResult CallExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    bytecode::Operand callee = TRY(ensure(state, *m_callee, {}));
    Type* type = state.type(callee);

    FunctionType const* function_type = nullptr;
    if (type->is_pointer()) {
        Type* pointee = type->get_pointee_type();
        if (!pointee->is_function()) {
            return err(span(), "Cannot call a value of type '{0}'", type->str());
        }

        function_type = pointee->as<FunctionType>();
    } else if (type->is_function()) {
        function_type = type->as<FunctionType>();
    } else {
        return err(span(), "Cannot call a value of type '{0}'", type->str());
    }

    size_t params = function_type->parameters().size();

    if (function_type->is_var_arg() && m_args.size() < params) {
        return err(span(), "Expected at least {0} arguments but got {1}", params, m_args.size());
    } else if (!function_type->is_var_arg() && m_args.size() != params) {
        return err(span(), "Expected {0} arguments but got {1}", params, m_args.size());
    }

    Vector<bytecode::Operand> arguments;
    for (auto [index, arg] : llvm::enumerate(m_args)) {
        auto operand = TRY(ensure(state, *arg, {}));
        if (index >= params) {
            arguments.push_back(operand);
            continue;
        }

        Type* parameter_type = function_type->get_parameter_at(index);

        operand = TRY(state.type_check_and_cast(arg->span(), operand, parameter_type, "Cannot pass a value of type '{0}' to a parameter that expects '{1}'"));
        arguments.push_back(operand);
    }

    auto reg = select_dst(state, dst);
    state.emit<bytecode::Call>(reg, callee, function_type, arguments);

    state.set_register_type(reg, function_type->return_type());
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
        operand = TRY(state.type_check_and_cast(m_value->span(), operand, return_type, "Cannot return a value of type '{0}' from a function that expects '{1}'"));

        state.emit<bytecode::Return>(operand);
    } else {
        if (!return_type->is_void()) {
            return err(span(), "Cannot return void from a function that expects '{0}'", return_type->str());
        }

        state.emit<bytecode::Return>();
    }

    return {};
}

BytecodeResult FunctionDeclExpr::generate(State& state, Optional<bytecode::Register>) const {
    Vector<FunctionParameter> parameters;
    for (auto [index, param] : llvm::enumerate(m_parameters)) {
        Type* type = TRY(param.type->evaluate(state));
        parameters.push_back({ param.name, type, param.flags, static_cast<u32>(index), param.span });
    }

    Type* return_type = state.types().void_type();
    if (m_return_type) {
        return_type = TRY(m_return_type->evaluate(state));
    }

    auto range = llvm::map_range(parameters, [](auto& param) { return param.type; });
    auto* underlying_type = state.types().create_function_type(return_type, Vector<Type*>(range.begin(), range.end()), m_is_c_variadic);

    auto* scope = Scope::create(m_name, ScopeType::Function, state.scope());
    auto function = Function::create(m_name, parameters, underlying_type, scope);

    state.scope()->add_symbol(function);
    state.add_global_function(function);

    state.emit<bytecode::NewFunction>(&*function);
    return {};
}

BytecodeResult FunctionExpr::generate(State& state, Optional<bytecode::Register>) const {
    TRY(m_decl->generate(state, {}));
    auto* function = state.scope()->resolve<Function>(m_decl->name());

    auto* entry_block = state.create_block();
    function->set_entry_block(entry_block);
    
    auto* previous_block = state.current_block();
    state.switch_to(entry_block);

    Scope* previous_scope = state.scope();
    for (auto& param : function->parameters()) {
        size_t index = function->allocate_local();
        function->set_local_type(index, param.type);

        auto variable = Variable::create(param.name, index, param.type);
        function->scope()->add_symbol(variable);
    }
    
    state.set_current_scope(function->scope());
    state.set_current_function(function);

    state.emit<bytecode::NewLocalScope>(function);
    for (auto& expr : m_body) {
        TRY(expr->generate(state, {}));
    }

    state.switch_to(previous_block);
    state.set_current_scope(previous_scope);

    return {};
}

BytecodeResult DeferExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult IfExpr::generate(State& state, Optional<bytecode::Register>) const {
    Function* current_function = state.function();

    auto* then_block = state.create_block();
    auto* else_block = state.create_block();

    auto operand = TRY(ensure(state, *m_condition, {}));
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
            state.switch_to(end_block);
            current_function->insert_block(end_block);
        }
    } else {
        state.switch_to(else_block);
    }

    return {};
}

BytecodeResult WhileExpr::generate(State& state, Optional<bytecode::Register>) const {
    Function* current_function = state.function();
    auto operand = TRY(ensure(state, *m_condition, {}));

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
        auto type = state.types().create_struct_type(m_name, {});
        auto structure = Struct::create(m_name, type);

        state.scope()->add_symbol(structure);
        state.emit<bytecode::NewStruct>(&*structure);

        return {};
    }

    HashMap<String, quart::StructField> fields;
    for (auto& field : m_fields) {
        Type* type = TRY(field.type->evaluate(state));
        fields.insert_or_assign(field.name, quart::StructField { field.name, type, field.flags, field.index });
    }

    auto range = llvm::map_range(fields, [](auto& entry) { return entry.second.type; });
    StructType* type = state.types().create_struct_type(m_name, Vector<Type*>(range.begin(), range.end()));

    auto* scope = Scope::create(m_name, ScopeType::Struct, state.scope());

    auto structure = Struct::create(m_name, type, move(fields), scope);
    state.scope()->add_symbol(structure);

    Scope* previous_scope = state.scope();

    state.set_current_scope(scope);
    state.set_current_struct(&*structure);

    state.add_global_struct(structure);
    for (auto& expr : m_members) {
        TRY(expr->generate(state, {}));
    }

    state.set_current_scope(previous_scope);
    state.emit<bytecode::NewStruct>(&*structure);

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
            return err(argument.span, "Unknown field '{0}' for struct '{1}'", argument.name, structure->name());
        }

        auto& field = iterator->second;
        auto operand = TRY(ensure(state, *argument.value, {}));

        arguments[field.index] = operand;
    }

    auto reg = select_dst(state, dst);
    state.emit<bytecode::Construct>(reg, structure, arguments);

    state.set_register_type(reg, structure->underlying_type());
    return bytecode::Operand(reg);
}

BytecodeResult EmptyConstructorExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult AttributeExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    return TRY(state.generate_attribute_access(*this, false, dst));
}

BytecodeResult IndexExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult CastExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult SizeofExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult OffsetofExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult PathExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult TupleExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult EnumExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult ImportExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult UsingExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult ModuleExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult TernaryExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult ForExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult RangeForExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult ArrayFillExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult TypeAliasExpr::generate(State& state, Optional<bytecode::Register>) const {
    Vector<GenericTypeParameter> parameters;
    bool is_generic = !m_parameters.empty();
    
    for (auto& param : m_parameters) {
        Vector<Type*> constraints;
        for (auto& constraint : param.constraints) {
            constraints.push_back(TRY(constraint->evaluate(state)));
        }

        Type* default_type = param.default_type ? TRY(param.default_type->evaluate(state)) : nullptr;
        parameters.push_back({ param.name, constraints, default_type, param.span });
    }

    if (is_generic) {
        auto alias = TypeAlias::create(m_name, parameters, &*m_type);
        state.scope()->add_symbol(alias);

        return {};
    }

    Type* underlying_type = TRY(m_type->evaluate(state));
    auto alias = TypeAlias::create(m_name, underlying_type);

    state.scope()->add_symbol(alias);
    return {};
}

BytecodeResult StaticAssertExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult MaybeExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult MatchExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult ImplExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

}