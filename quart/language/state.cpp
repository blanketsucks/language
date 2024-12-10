#include <quart/language/state.h>

namespace quart {

State::State() {
    m_type_registry = TypeRegistry::create();
    m_current_scope = m_global_scope = Scope::create({}, ScopeType::Global, nullptr);
}

void State::dump() const {}

bytecode::Register State::allocate_register() {
    bytecode::Register reg = m_generator.allocate_register();
    m_registers.push_back({});

    return reg;
}

void State::switch_to(bytecode::BasicBlock* block) {
    m_generator.switch_to(block);
}

void State::set_register_state(bytecode::Register reg, Type* type, Function* function) {
    m_registers[reg.index()] = { type, function };
}

Type* State::type(bytecode::Register reg) const {
    return m_registers[reg.index()].type;
}

Type* State::type(bytecode::Operand operand) const {
    if (operand.is_immediate()) {
        return operand.value_type();
    } else {
        return m_registers[operand.value()].type;
    }
}

bool State::has_global_function(const String& name) const {
    return m_all_functions.contains(name);
}

void State::add_global_function(RefPtr<Function> function) {
    m_all_functions[function->qualified_name()] = function;
}

Function const& State::get_global_function(const String& name) const {
    return *m_all_functions.at(name);
}

void State::add_global_struct(RefPtr<Struct> structure) {
    m_all_structs[structure->underlying_type()] = structure;
}

void State::add_impl(OwnPtr<Impl> impl) {
    if (impl->is_generic()) {
        m_generic_impls.push_back(move(impl));
        return;
    }

    m_impls[impl->underlying_type()] = move(impl);
}

bool State::has_impl(Type* type) {
    return m_impls.contains(type);
}

Struct const* State::get_global_struct(Type* type) const {
    if (auto iterator = m_all_structs.find(type); iterator != m_all_structs.end()) {
        return iterator->second.get();
    }

    return nullptr;
}

bool State::has_global_module(const String& name) const {
    return m_modules.contains(name);
}

RefPtr<Module> State::get_global_module(const String& name) const {
    if (auto iterator = m_modules.find(name); iterator != m_modules.end()) {
        return iterator->second;
    }

    return nullptr;
}

void State::add_global_module(RefPtr<Module> module) {
    m_modules[module->qualified_name()] = move(module);
}

ErrorOr<Scope*> State::resolve_scope(Span span, Scope* current_scope, const String& name) {
    auto* symbol = current_scope->resolve(name);
    if (!symbol) {
        return err(span, "namespace '{0}' not found", name);
    }

    if (!symbol->is(Symbol::Module, Symbol::Struct)) {
        return err(span, "'{0}' is not a valid namespace", name);
    }

    Scope* scope = nullptr;
    if (symbol->is<Module>()) {
        scope = symbol->as<Module>()->scope();
    } else {
        scope = symbol->as<Struct>()->scope();
    }

    return scope;
}

ErrorOr<Scope*> State::resolve_scope_path(Span span, const Path& path, bool allow_generic_arguments) {
    Scope* scope = m_current_scope;

    for (auto& [segment, arguments] : path.segments) {
        if (!arguments.empty() && !allow_generic_arguments) {
            return err(span, "Generic arguments are not allowed in this context");
        }

        Span segment_span = { span.start(), span.start() + segment.size(), span.source_code_index() };
        scope = TRY(this->resolve_scope(segment_span, scope, segment));

        span.set_start(segment_span.end() + 2);
    }

    return scope;
}

ErrorOr<bytecode::Register> State::resolve_reference(Scope* scope, Span span, const String& name, bool is_mutable, Optional<bytecode::Register> dst) {
    auto* symbol = scope->resolve(name);
    if (!symbol) {
        return err(span, "Unknown identifier '{0}'", name);
    }

    bytecode::Register reg;
    if (dst.has_value()) {
        reg = *dst;
    } else {
        reg = this->allocate_register();
    }

    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = symbol->as<Variable>();
            emit<bytecode::GetLocalRef>(reg, variable->index());

            if (!variable->is_mutable() && is_mutable) {
                return err(span, "Cannot take a mutable reference to an immutable variable");
            }

            Type* type = variable->value_type();
            this->set_register_state(reg, type->get_reference_to(is_mutable));

            return reg;
        }
        default:
            return err(span, "Invalid reference");
    }
}

ErrorOr<bytecode::Register> State::resolve_reference(ast::Expr const& expr, bool is_mutable, Optional<bytecode::Register> dst, bool use_default_case) {
    using ast::ExprKind;
    
    switch (expr.kind()) {
        case ExprKind::Identifier: {
            auto* ident = expr.as<ast::IdentifierExpr>();
            return this->resolve_reference(m_current_scope, expr.span(), ident->name(), is_mutable, dst);
        }
        case ExprKind::Path: {
            auto& path = expr.as<ast::PathExpr>()->path();
            Scope* scope = TRY(this->resolve_scope_path(expr.span(), path));

            return this->resolve_reference(scope, expr.span(), path.last.name, is_mutable, dst);
        }
        case ExprKind::Attribute: {
            auto* attribute = expr.as<ast::AttributeExpr>();
            auto operand = TRY(this->generate_attribute_access(*attribute, true, is_mutable, dst));

            ASSERT(operand.is_register(), {});
            return bytecode::Register(operand.value());
        }
        case ExprKind::Index: {
            auto index = expr.as<ast::IndexExpr>();
            auto operand = TRY(this->generate_index_access(*index, true, is_mutable, dst));

            ASSERT(operand.is_register(), {});
            return bytecode::Register(operand.value());
        }
        default:
            if (!use_default_case) {
                return err(expr.span(), "Invalid reference");
            }

            auto option = TRY(expr.generate(*this, {}));
            if (!option.has_value()) {
                return err(expr.span(), "Expected an expression");
            }

            Type* type = this->type(*option);
            if (!type->is_reference()) {
                return err(expr.span(), "Expected a reference type but got '{0}'", type->str());
            }

            auto reg = this->allocate_register();
            emit<bytecode::Move>(reg, *option);

            this->set_register_state(reg, type);
            return reg;
    }

    return {};
}

ErrorOr<Symbol*> State::resolve_symbol(ast::Expr const& expr) {
    using ast::ExprKind;
    
    switch (expr.kind()) {
        case ExprKind::Identifier: {
            auto* identifier = expr.as<ast::IdentifierExpr>();
            auto* symbol = m_current_scope->resolve(identifier->name());

            if (!symbol) {
                return err(expr.span(), "Unknown identifier '{0}'", identifier->name());
            }

            return symbol;
        }
        case ExprKind::Path: {
            auto& path = expr.as<ast::PathExpr>()->path();
            Scope* scope = TRY(this->resolve_scope_path(expr.span(), path));

            auto* symbol = scope->resolve(path.last.name);
            if (!symbol) {
                return err(expr.span(), "Unknown identifier '{0}'", path.last.name);
            }

            return symbol;
        }
        default:
            return err(expr.span(), "Expected an identifier");
    }
}

ErrorOr<Struct*> State::resolve_struct(ast::Expr const& expr) {
    auto* symbol = TRY(this->resolve_symbol(expr));
    if (!symbol->is<Struct>()) {
        return err(expr.span(), "'{0}' does not name a struct", symbol->name());
    }

    return symbol->as<Struct>();
}

ErrorOr<bytecode::Operand> State::type_check_and_cast(Span span, bytecode::Operand operand, Type* target, StringView error_message) {
    Type* type = this->type(operand);
    if (!type->can_safely_cast_to(target)) {
        return err(span, error_message.data(), type->str(), target->str());
    } else if (type == target) {
        return operand;
    }

    // If the only difference between these two types is the mutability we don't need to emit a Cast instruction as the underlying code generators
    // don't care about that.
    if ((type->is_pointer() || type->is_reference()) && (target->is_pointer() || target->is_reference())) {
        if (type->underlying_type() == target->underlying_type()) {
            return operand;
        }
    }

    auto reg = this->allocate_register();
    emit<bytecode::Cast>(reg, operand, target);

    this->set_register_state(reg, target);
    return bytecode::Operand(reg);
}

ErrorOr<bytecode::Operand> State::generate_attribute_access(
    ast::AttributeExpr const& expr, bool as_reference, bool as_mutable, Optional<bytecode::Register> dst
) {
    ast::Expr const& parent = expr.parent();
    auto result = this->resolve_symbol(parent);

    bool is_mutable = false;
    bool is_pointer_type = false;

    bytecode::Register reg = this->allocate_register();
    bool reg_has_value = false;

    Type* variable_type = nullptr;
    Type* value_type = nullptr;

    size_t local_index = 0;

    if (result.is_err()) {
        auto option = TRY(parent.generate(*this, {}));
        if (!option.has_value()) {
            return err(parent.span(), "Expected an expression");
        }

        value_type = this->type(*option);
        if (value_type->is_pointer() || value_type->is_reference()) {
            this->set_register_state(reg, value_type);

            value_type = value_type->underlying_type();
            emit<bytecode::Move>(reg, *option);
        } else {
            emit<bytecode::Alloca>(reg, value_type);
            emit<bytecode::Write>(reg, *option);

            this->set_register_state(reg, value_type->get_pointer_to());
        }
        
        reg_has_value = true;
    } else {
        auto* symbol = result.value();
        if (!symbol->is<Variable>()) {
            return err(parent.span(), "Attribute access is only allowed on variables");
        }

        auto* variable = symbol->as<Variable>();

        variable_type = variable->value_type();
        value_type = variable->value_type();

        if (value_type->is_pointer() || value_type->is_reference()) {
            value_type = value_type->underlying_type();
            is_pointer_type = true;
        }

        local_index = variable->index();
        is_mutable = variable->is_mutable();
    }

    auto* structure = this->get_global_struct(value_type);
    Scope* scope = nullptr;

    if (!structure) {
        Type* ty = variable_type ? variable_type : value_type;
        if (!this->has_impl(ty)) {
            for (auto& impl : m_generic_impls) {
                scope = TRY(impl->make(*this, ty));
                if (scope) {
                    break;
                }
            }

            if (!scope) {
                return err(parent.span(), "Cannot access attributes of type '{0}'", ty->str());
            }
        } else {
            auto& impl = *m_impls[ty];
            scope = impl.scope();
        }
    } else {
        scope = structure->scope();
    }

    if (!reg_has_value) {
        if (is_pointer_type) {
            emit<bytecode::GetLocal>(reg, local_index);
            this->set_register_state(reg, variable_type);
        } else {
            emit<bytecode::GetLocalRef>(reg, local_index);
            this->set_register_state(reg, variable_type->get_pointer_to());
        }
    }

    auto& attr = expr.attribute();
    auto* method = scope->resolve<Function>(attr);

    if (method) {
        if (!dst) {
            dst = this->allocate_register();
        }

        auto& self = method->parameters().front();
        if (self.is_mutable() && !is_mutable) {
            return err(parent.span(), "Function '{0}' requires a mutable reference to self but self is immutable", method->name());
        }

        emit<bytecode::GetFunction>(*dst, method);
        this->set_register_state(*dst, method->underlying_type()->get_pointer_to(), method);

        this->inject_self(reg);
        return bytecode::Operand(*dst);
    }

    if (!structure) {
        return err("Type '{0}' has no attribute named '{1}'", value_type->str(), attr);
    }

    auto* field = structure->find(attr);
    if (!field) {
        return err(expr.span(), "Unknown attribute '{0}' for struct '{1}'", attr, structure->name());
    }

    if (as_reference && as_mutable && !is_mutable) {
        return err(parent.span(), "Cannot take a mutable reference to an immutable value");
    }

    if (!dst.has_value()) {
        dst = this->allocate_register();
    }

    bytecode::Operand index(field->index, types().i32());
    if (as_reference) {
        emit<bytecode::GetMemberRef>(*dst, bytecode::Operand(reg), index);
        this->set_register_state(*dst, field->type->get_reference_to(as_mutable));
    } else {
        emit<bytecode::GetMember>(*dst, bytecode::Operand(reg), index);
        this->set_register_state(*dst, field->type);
    }

    return bytecode::Operand(*dst);
}

ErrorOr<bytecode::Operand> State::generate_index_access(
    ast::IndexExpr const& expr, bool as_reference, bool as_mutable, Optional<bytecode::Register> dst
) {
    auto result = this->resolve_reference(expr.value(), as_mutable, {}, false);

    bytecode::Register reg;
    Type* type = nullptr;

    bool deref = false;
    if (result.is_err()) {
        auto option = TRY(expr.value().generate(*this, {}));
        if (!option.has_value()) {
            return err(expr.value().span(), "Expected an expression");
        }

        type = this->type(*option);
        if (!type->is_array() && !type->is_pointer()) {
            return err(expr.value().span(), "Cannot index into type '{0}'", type->str());
        }

        reg = this->allocate_register();
        if (type->is_pointer()) {
            emit<bytecode::Move>(reg, *option);
        } else {
            // FIXME: Use extractvalue in the LLVM backend for arrays
            return err(expr.value().span(), "Indexing into array immediates is not yet supported");
        }
    } else {
        reg = result.value();
        type = this->type(reg)->get_reference_type();

        if (!type->is_array() && !type->is_pointer()) {
            return err(expr.span(), "Cannot index into type '{0}'", type->str());
        }

        deref = true;
    }

    Type* inner = nullptr;
    if (type->is_array()) {
        inner = type->get_array_element_type();
    } else if (type->is_pointer() && deref) {
        emit<bytecode::Read>(reg, reg);
        inner = type->get_pointee_type();
    } else {
        inner = type->get_pointee_type();
    }

    auto index = TRY(expr.index().generate(*this, {}));
    if (!index.has_value()) {
        return err(expr.index().span(), "Expected an expression");
    }

    auto idx = index.value();
    if (!this->type(idx)->is_int()) {
        return err(expr.index().span(), "Expected an integer");
    }

    if (!dst.has_value()) {
        dst = this->allocate_register();
    }

    // GetMemberRef/GetMember expects a pointer
    this->set_register_state(reg, type->get_pointer_to());
    if (as_reference) {
        emit<bytecode::GetMemberRef>(*dst, bytecode::Operand(reg), idx);
        this->set_register_state(*dst, inner->get_reference_to(as_mutable));
    } else {
        emit<bytecode::GetMember>(*dst, bytecode::Operand(reg), idx);
        this->set_register_state(*dst, inner);
    }

    return bytecode::Operand(*dst);
}

fs::Path State::search_import_paths(const String& name) {
    static const Vector<fs::Path> IMPORT_PATHS = { fs::Path(QUART_PATH) };

    for (auto& path : IMPORT_PATHS) {
        fs::Path fullpath = path / name;
        if (fullpath.exists()) {
            return fullpath;
        }
    }

    return {};
}

}
