#include <quart/language/state.h>
#include <quart/parser/parser.h>
#include <quart/parser/ast.h>
#include <quart/temporary_change.h>

namespace quart::ast {

struct ModuleQualifiedName {
    String name;

    explicit ModuleQualifiedName(String name) : name(move(name)) {}

    operator String() const { return name; }

    void append(String const& segment) {
        name.append("::");
        name.append(segment);
    }
};

static String format_path(Path const& path) {
    String result = path.name;
    for (auto& segment : path.segments) {
        result.append("::");
        result.append(segment);
    }

    return result;
}

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

BytecodeResult ExternBlockExpr::generate(State& state, Optional<bytecode::Register>) const {
    for (auto& expr : m_block) {
        TRY(expr->generate(state, {}));
    }

    return {};
}

BytecodeResult IntegerExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    IntType* type = nullptr;
    Type* context = state.context();

    outln("context = {0}, value = {1}", context, m_value);
    if (context && context->is_int()) {
        type = const_cast<IntType*>(context->as<IntType>());
    } else {
        type = state.types().create_int_type(m_width, true);
    }

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

            variable->emit(state, reg);
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
    auto variable = Variable::create(m_name, global_index, state.type(value), Variable::Constant);

    state.emit<bytecode::SetGlobal>(global_index, value);
    state.scope()->add_symbol(variable);

    return {};
}

BytecodeResult UnaryOpExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto reg = select_dst(state, dst);
    switch (m_op) {
        case UnaryOp::Not:
            break;
        case UnaryOp::DeRef: {
            bytecode::Operand value = TRY(ensure(state, *m_value, {}));
            Type* type = state.type(value);

            if (!type->is_pointer() && !type->is_reference()) {
                return err(span(), "Cannot de-reference value of type '{0}'", type->str());
            }

            state.emit<bytecode::Read>(reg, bytecode::Register(value.value()));
            state.set_register_type(reg, type->underlying_type());

            return bytecode::Operand(reg);
        }
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

BytecodeResult InplaceBinaryOpExpr::generate(State& state, Optional<bytecode::Register>) const {
    auto ref = TRY(state.resolve_reference(*m_lhs, true));
    Type* type = state.type(ref)->get_reference_type();

    auto lhs = state.allocate_register();
    state.emit<bytecode::Read>(lhs, ref);

    auto rhs = TRY(ensure(state, *m_rhs, {}));
    rhs = TRY(state.type_check_and_cast(span(), rhs, type, "Cannot assign a value of type '{0}' to a variable of type '{1}'"));

    auto reg = state.allocate_register();
    switch (m_op) {
        // NOLINTNEXTLINE
        #define Op(x) case BinaryOp::x: state.emit<bytecode::x>(reg, bytecode::Operand(lhs), rhs); break;
            ENUMERATE_BINARY_OPS(Op)
        #undef Op

        default:
            return err(span(), "Unknown binary operator");
    }

    state.emit<bytecode::Write>(ref, bytecode::Operand(reg));
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

    Optional<bytecode::Operand> self = state.self();

    size_t index = 0;
    size_t params = function_type->parameters().size();

    // The only case in which `self` would be not None is when calling a method so we know for sure that we can just ignore the first parameter.
    if (self.has_value()) {
        params--;
        index++;
    }

    if (function_type->is_var_arg() && m_args.size() < params) {
        return err(span(), "Expected at least {0} arguments but got {1}", params, m_args.size());
    } else if (!function_type->is_var_arg() && m_args.size() != params) {
        return err(span(), "Expected {0} arguments but got {1}", params, m_args.size());
    }

    Vector<bytecode::Operand> arguments;
    if (self.has_value()) {
        arguments.push_back(self.value());
        state.reset_self();
    }

    for (auto& arg : m_args) {
        if (index >= params && function_type->is_var_arg()) {
            auto operand = TRY(ensure(state, *arg, {}));
            arguments.push_back(operand);

            continue;
        }

        Type* parameter_type = function_type->get_parameter_at(index);
        state.set_type_context(parameter_type);

        auto operand = TRY(ensure(state, *arg, {}));

        operand = TRY(state.type_check_and_cast(arg->span(), operand, parameter_type, "Cannot pass a value of type '{0}' to a parameter that expects '{1}'"));
        arguments.push_back(operand);

        state.set_type_context(nullptr);
        index++;
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
    Type* self_type = state.self_type();

    for (auto [index, param] : llvm::enumerate(m_parameters)) {
        Type* type = nullptr;
        if (self_type && param.flags & FunctionParameter::Self) {
            type = self_type->get_pointer_to(param.flags & FunctionParameter::Mutable);
        } else {
            type = TRY(param.type->evaluate(state));
        }

        parameters.push_back({ param.name, type, param.flags, static_cast<u32>(index), param.span });
    }

    Type* return_type = state.types().void_type();
    if (m_return_type) {
        return_type = TRY(m_return_type->evaluate(state));
    }

    auto range = llvm::map_range(parameters, [](auto& param) { return param.type; });
    auto* underlying_type = state.types().create_function_type(return_type, Vector<Type*>(range.begin(), range.end()), m_is_c_variadic);

    auto* scope = Scope::create(m_name, ScopeType::Function, state.scope());
    auto function = Function::create(m_name, parameters, underlying_type, scope, m_linkage);

    state.scope()->add_symbol(function);
    state.add_global_function(function);

    state.emit<bytecode::NewFunction>(&*function);
    return {};
}

BytecodeResult FunctionExpr::generate(State& state, Optional<bytecode::Register>) const {
    TRY(m_decl->generate(state, {}));
    auto* function = state.scope()->resolve<Function>(m_decl->name());

    auto* previous_function = state.function();

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

    state.set_current_function(previous_function);
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
        auto structure = Struct::create(m_name, type, state.scope());

        state.scope()->add_symbol(structure);
        state.emit<bytecode::NewStruct>(&*structure);

        return {};
    }

    StructType* type = state.types().create_struct_type(m_name, {});
    auto* scope = Scope::create(m_name, ScopeType::Struct, state.scope());

    auto structure = Struct::create(m_name, type, {}, scope);
    state.scope()->add_symbol(structure);

    HashMap<String, quart::StructField> fields;
    Vector<Type*> types;

    for (auto& field : m_fields) {
        Type* type = TRY(field.type->evaluate(state));
        fields.insert_or_assign(field.name, quart::StructField { field.name, type, field.flags, field.index });

        types.push_back(type);
    }

    type->set_fields(types);
    structure->set_fields(move(fields));

    Scope* previous_scope = state.scope();

    state.set_current_scope(scope);
    state.set_current_struct(&*structure);

    state.add_global_struct(structure);
    state.set_self_type(structure->underlying_type());

    for (auto& expr : m_members) {
        TRY(expr->generate(state, {}));
    }

    state.set_current_scope(previous_scope);
    state.emit<bytecode::NewStruct>(&*structure);

    state.set_self_type(nullptr);
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

BytecodeResult CastExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    auto value = TRY(ensure(state, *m_value, {}));
    Type* type = TRY(m_to->evaluate(state));

    // FIXME: More checks are needed to be put in place and we can't really use State::type_check_and_cast here
    // because it does a "safe" cast.

    auto reg = select_dst(state, dst);
    state.emit<bytecode::Cast>(reg, value, type);

    state.set_register_type(reg, type);
    return bytecode::Operand(reg);
}

BytecodeResult SizeofExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult OffsetofExpr::generate(State&, Optional<bytecode::Register>) const {
    ASSERT(false && "Not implemented");
    return {};
}

BytecodeResult PathExpr::generate(State& state, Optional<bytecode::Register> dst) const {
    Scope* scope = TRY(state.resolve_scope_path(span(), m_path));
    auto* symbol = scope->resolve(m_path.name);

    if (!symbol) {
        return err(span(), "Unknown identifier '{0}'", m_path.name);
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

            state.set_register_type(reg, function->underlying_type()->get_pointer_to());
            return bytecode::Operand(reg);
        }
        default:
            return err(span(), "'{0}' does not refer to a value", m_path.name);
    }

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

BytecodeResult ImportExpr::generate(State& state, Optional<bytecode::Register>) const {
    String qualified_name = format_path(m_path);

    auto module = state.get_global_module(qualified_name);
    Scope* current_scope = state.scope();

    Scope* prev_scope = current_scope;
    if (module) {
        if (module->is_importing()) {
            return err(span(), "Could not import '{0}' because a circular dependency was detected", m_path.name);
        }

        current_scope->add_symbol(module);
        return {};
    }

    String fullpath = {};
    ModuleQualifiedName current_qualified_name(m_path.name);

    for (auto& segment : m_path.segments) {
        fullpath.append(segment);
        fs::Path path(fullpath);
        
        if (!path.exists()) {
            path = state.search_import_paths(fullpath);

            if (path.empty()) {
                return err(span(), "Could not find module '{0}'", m_path.name);
            }

            fullpath = fullpath.substr(0, fullpath.size() - segment.size()) + String(path);
        }

        if (!path.is_dir()) {
            return err(span(), "Expected a directory, got a file");
        }

        auto* module = current_scope->resolve<Module>(segment);
        Scope* new_scope = nullptr;

        if (!module) {
            RefPtr<Module> mod = nullptr;
            if (state.has_global_module(current_qualified_name)) {
                mod = state.get_global_module(current_qualified_name);
            } else {
                Scope* scope = Scope::create(segment, ScopeType::Module, current_scope);
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

        current_qualified_name.append(segment);
    }

    fs::Path path = fs::Path(fullpath + m_path.name + FILE_EXTENSION);
    String name = path;

    if (!path.exists()) {
        fs::Path dir = path.with_extension();
        if (!dir.exists()) {
            dir = state.search_import_paths(dir);
            if (dir.empty()) {
                return err(span(), "Could not find module '{0}'", m_path.name);
            }
        }

        if (!dir.is_dir()) {
            return err(span(), "Expected a directory, got a file");
        }

        name = dir;
        path = dir.join("module.qr");

        if (!path.exists()) {
            Scope* scope = Scope::create(m_path.name, ScopeType::Module, current_scope);
            auto module = Module::create(m_path.name, qualified_name, path, scope);

            current_scope->add_symbol(module);
            state.add_global_module(module);

            return {};
        }

        if (!path.is_regular_file()) {
            err(span(), "Expected a file, got a directory");
        }
    }

    auto* prev_module = state.module();
    current_scope = Scope::create(m_path.name, ScopeType::Module, current_scope);

    module = Module::create(m_path.name, qualified_name, path, current_scope);
    prev_scope->add_symbol(module);

    state.add_global_module(module);

    state.set_current_scope(current_scope);
    state.set_current_module(&*module);

    SourceCode code = SourceCode::from_path(path);
    Lexer lexer(code);

    Vector<Token> tokens = ({
        auto result = lexer.lex();
        if (result.is_err()) {
            auto& error = result.error();
            errln("{0}", code.format_error(error));

            return err(span(), "Failed to import module '{0}'", m_path.name);
        }

        result.release_value();
    });

    Parser parser(move(tokens));
    auto ast = ({
        auto result = parser.parse();
        if (result.is_err()) {
            auto& error = result.error();
            errln("{0}", code.format_error(error));

            return err(span(), "Failed to import module '{0}'", m_path.name);
        }

        result.release_value();
    });

    for (auto& expr : ast) {
        TRY(expr->generate(state, {}));
    }

    state.set_current_scope(prev_scope);
    state.set_current_module(prev_module);

    return {};
}

BytecodeResult UsingExpr::generate(State&, Optional<bytecode::Register>) const {
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

BytecodeResult RangeForExpr::generate(State& state, Optional<bytecode::Register>) const {
    Function* current_function = state.function();
    Scope* current_scope = state.scope();

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

    state.set_register_type(reg, type);
    state.emit<bytecode::GetLocal>(reg, local_index);

    state.emit<bytecode::Add>(reg, bytecode::Operand(reg), bytecode::Operand(1, type));
    state.emit<bytecode::SetLocal>(local_index, bytecode::Operand(reg));

    if (m_end) {
        if (m_inclusive) {
            state.emit<bytecode::Lt>(reg, *end, bytecode::Operand(reg));
        } else {
            state.emit<bytecode::Eq>(reg, *end, bytecode::Operand(reg));
        }

        state.emit<bytecode::JumpIf>(bytecode::Operand(reg), end_block, body_block);
    } else {
        state.emit<bytecode::Jump>(body_block);
    }

    current_function->insert_block(end_block);
    state.switch_to(end_block);
    
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

String extract_name_from_type(ast::TypeExpr const& type) {
    if (type.kind() != TypeKind::Named) {
        return {};
    }

    auto* named = type.as<ast::NamedTypeExpr>();
    auto& path = named->path();

    return path.segments.empty() ? path.name : String();
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
    if (!m_parameters.empty()) {
        auto range = llvm::map_range(m_parameters, [](auto& param) { return param.name; });
        Set<String> parameters(range.begin(), range.end());

        Vector<OwnPtr<ImplCondition>> conditions;
        create_impl_conditions(conditions, parameters, *m_type);

        if (conditions.empty()) {
            return err(span(), "Impl is generic but doesn't use generic parameters");
        }
        
        auto impl = Impl::create(&*m_type, &*m_body, move(conditions));
        state.add_impl(move(impl));

        return {};
    }

    Type* underlying_type = TRY(m_type->evaluate(state));

    Scope* current_scope = state.scope();
    Scope* scope = Scope::create(underlying_type->str(), ScopeType::Impl, current_scope);

    auto impl = Impl::create(underlying_type, scope);
    state.set_self_type(impl->underlying_type());

    state.set_current_scope(scope);
    TRY(m_body->generate(state, {}));

    state.set_current_scope(current_scope);
    state.add_impl(move(impl));

    state.set_self_type(nullptr);
    return {};
}

}