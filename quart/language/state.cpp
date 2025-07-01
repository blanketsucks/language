#include <quart/language/state.h>
#include <quart/parser/parser.h>

namespace quart {

State::State() : m_constant_evaluator(*this) {
    m_context = Context::create();
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

void State::set_register_state(bytecode::Register reg, Type* type, Function* function, u8 flags) {
    m_registers[reg.index()] = { type, function, flags };
}

Type* State::type(bytecode::Register reg) const {
    return m_registers[reg.index()].type;
}

Type* State::type(bytecode::Operand const& operand) const {
    if (operand.is_register()) {
        return m_registers[operand.value()].type;
    }

    return operand.value_type();
}

bool State::has_global_function(const String& name) const {
    return m_all_functions.contains(name);
}

void State::add_global_function(RefPtr<Function> function) {
    m_all_functions[function->qualified_name()] = function;
}

Function const* State::get_global_function(const String& name) const {
    auto iterator = m_all_functions.find(name);
    if (iterator != m_all_functions.end()) {
        return iterator->second.get();
    }

    return nullptr;
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

ErrorOr<RefPtr<Scope>> State::resolve_scope(Span span, Scope& current_scope, const String& name) {
    auto* symbol = current_scope.resolve(name);
    if (!symbol) {
        return err(span, "namespace '{}' not found", name);
    }

    if (!symbol->is(Symbol::Module, Symbol::Struct)) {
        return err(span, "'{}' is not a valid namespace", name);
    }

    RefPtr<Scope> scope = nullptr;
    if (symbol->is<Module>()) {
        scope = symbol->as<Module>()->scope();
    } else {
        scope = symbol->as<Struct>()->scope();
    }

    return scope;
}

ErrorOr<RefPtr<Scope>> State::resolve_scope_path(Span span, const Path& path, bool allow_generic_arguments) {
    auto scope = m_current_scope;

    for (auto& segment : path.segments()) {
        if (segment.has_generic_arguments() && !allow_generic_arguments) {
            return err(span, "Generic arguments are not allowed in this context");
        }

        auto& name = segment.name();

        Span segment_span = { span.start(), span.start() + name.size(), span.source_code_index() };
        scope = TRY(this->resolve_scope(segment_span, *scope, name));

        span.set_start(segment_span.end() + 2);
    }

    return scope;
}

ErrorOr<bytecode::Register> State::resolve_reference(
    Scope& scope,
    Span span,
    const String& name,
    bool is_mutable,
    Optional<bytecode::Register> dst,
    bool override_mutability
) {
    auto* symbol = scope.resolve(name);
    if (!symbol) {
        return err(span, "Unknown identifier '{}'", name);
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
                return err(ErrorType::MutabilityMismatch, span, "Cannot take a mutable reference to an immutable variable");
            }

            Type* type = variable->value_type();
            if (override_mutability) {
                this->set_register_state(reg, type->get_reference_to(variable->is_mutable()));
            } else {
                this->set_register_state(reg, type->get_reference_to(is_mutable));
            }

            return reg;
        }
        default:
            return err(span, "Invalid reference");
    }
}

ErrorOr<bytecode::Register> State::resolve_reference(
    ast::Expr const& expr,
    bool is_mutable,
    Optional<bytecode::Register> dst,
    bool use_default_case,
    bool override_mutability
) {
    using ast::ExprKind;
    
    switch (expr.kind()) {
        case ExprKind::Identifier: {
            auto* ident = expr.as<ast::IdentifierExpr>();
            return this->resolve_reference(*m_current_scope, expr.span(), ident->name(), is_mutable, dst, override_mutability);
        }
        case ExprKind::Path: {
            auto& path = expr.as<ast::PathExpr>()->path();
            auto scope = TRY(this->resolve_scope_path(expr.span(), path));

            return this->resolve_reference(*scope, expr.span(), path.name(), is_mutable, dst, override_mutability);
        }
        case ExprKind::Attribute: {
            auto* attribute = expr.as<ast::AttributeExpr>();
            return TRY(this->generate_attribute_access(*attribute, true, is_mutable, dst));
        }
        case ExprKind::Index: {
            auto index = expr.as<ast::IndexExpr>();
            return TRY(this->generate_index_access(*index, true, is_mutable, dst));
        }
        default: {
            if (!use_default_case) {
                return err(expr.span(), "Invalid reference");
            }

            auto option = TRY(expr.generate(*this, {}));
            if (!option.has_value()) {
                return err(expr.span(), "Expected an expression");
            }

            Type* type = this->type(*option);
            if (!type->is_reference()) {
                return err(expr.span(), "Expected a reference type but got '{}'", type->str());
            }

            if (is_mutable && !type->is_mutable()) {
                return err(ErrorType::MutabilityMismatch, expr.span(), "Cannot take a mutable reference to an immutable value");
            }

            return *option;
        }
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
                return err(expr.span(), "Unknown identifier '{}'", identifier->name());
            }

            return symbol;
        }
        case ExprKind::Path: {
            auto& path = expr.as<ast::PathExpr>()->path();
            auto scope = TRY(this->resolve_scope_path(expr.span(), path));

            auto* symbol = scope->resolve(path.name());
            if (!symbol) {
                return err(expr.span(), "Unknown identifier '{}'", path.name());
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
        return err(expr.span(), "'{}' does not name a struct", symbol->name());
    }

    return symbol->as<Struct>();
}

ErrorOr<bytecode::Register> State::type_check_and_cast(Span span, bytecode::Register value, Type* target, StringView error_message) {
    Type* type = this->type(value);
    if (!type->can_safely_cast_to(target)) {
        String error = dyn_format(error_message, type->str(), target->str());
        return Error { span, move(error) };
    } else if (type == target) {
        return value;
    }

    // If the only difference between these two types is the mutability we don't need to emit a Cast instruction as the underlying code generators
    // don't care about that.
    if ((type->is_pointer() || type->is_reference()) && (target->is_pointer() || target->is_reference())) {
        if (type->underlying_type() == target->underlying_type()) {
            return value;
        }
    }

    // FIXME: Maybe use the same value as a dst for the Cast?
    auto reg = this->allocate_register();
    emit<bytecode::Cast>(reg, value, target);

    this->set_register_state(reg, target);
    return reg;
}

ErrorOr<bytecode::Register> State::generate_attribute_access(
    ast::AttributeExpr const& expr, bool as_reference, bool as_mutable, Optional<bytecode::Register> dst
) {
    ast::Expr const& parent = expr.parent();
    auto result = this->resolve_reference(parent, as_mutable, {}, false, true);

    bytecode::Register reg;
    Type* value_type = nullptr;

    bool is_mutable = false;
    if (result.is_err()) {
        auto& error = result.error();
        if (error.type() == ErrorType::MutabilityMismatch) {
            return error;
        }

        auto option = TRY(parent.generate(*this, {}));
        if (!option.has_value()) {
            return err(parent.span(), "Expected an expression");
        }

        bytecode::Register value = option.value();
        Type* type = this->type(value);

        if (!type->is_pointer() && !type->is_reference()) {
            value_type = type;
            reg = this->allocate_register();

            emit<bytecode::Alloca>(reg, type);
            emit<bytecode::Write>(reg, value);
        } else {
            reg = value;
            value_type = type->underlying_type();
        }
    } else {
        bytecode::Register value = result.value();

        Type* type = this->type(value);
        is_mutable = type->is_mutable();

        type = type->get_reference_type();
        if (type->is_pointer() || type->is_reference()) {
            value_type = type->underlying_type();

            reg = this->allocate_register();
            emit<bytecode::Read>(reg, value);
        } else {
            value_type = type;
            reg = value;
        }
    }

    this->set_register_state(reg, value_type->get_pointer_to());

    auto* structure = this->get_global_struct(value_type);
    RefPtr<Scope> scope = nullptr;

    if (!structure) {
        if (!this->has_impl(value_type)) {
            for (auto& impl : m_generic_impls) {
                scope = TRY(impl->make(*this, value_type));
                if (scope) {
                    break;
                }
            }

            if (!scope) {
                return err(parent.span(), "Cannot access attributes of type '{}'", value_type->str());
            }
        } else {
            auto& impl = *m_impls[value_type];
            scope = impl.scope();
        }
    } else {
        scope = structure->scope();
    }

    auto& attr = expr.attribute();
    auto* method = scope->resolve<Function>(attr);

    if (method) {
        // FIXME: Handle the case where the function comes from an impl not a struct
        if (!method->is_public() && m_current_struct != structure && method->module() != m_current_module) {
            return err(expr.span(), "Cannot access private method '{}' of struct '{}'", method->name(), structure->qualified_name());
        }

        if (!dst) {
            dst = this->allocate_register();
        }

        auto& self = method->parameters().front();
        if (self.is_mutable() && !is_mutable) {
            return err(parent.span(), "Function '{}' requires a mutable reference to self but self is immutable", method->name());
        }

        emit<bytecode::GetFunction>(*dst, method);
        this->set_register_state(*dst, method->underlying_type()->get_pointer_to(), method);

        this->inject_self(reg);
        return *dst;
    }

    if (!structure) {
        return err("Type '{}' has no attribute named '{}'", value_type->str(), attr);
    }

    auto* field = structure->find(attr);
    if (!field) {
        return err(expr.span(), "Unknown attribute '{}' for struct '{}'", attr, structure->name());
    } else if (!field->is_public() && m_current_struct != structure && structure->module() != m_current_module) {
        return err(expr.span(), "Cannot access private field '{}'", field->name);
    }

    if (!dst.has_value()) {
        dst = this->allocate_register();
    }

    bytecode::Operand index = { field->index, m_context->i32() };
    if (as_reference) {
        emit<bytecode::GetMemberRef>(*dst, reg, index);
        this->set_register_state(*dst, field->type->get_reference_to(as_mutable));
    } else {
        emit<bytecode::GetMember>(*dst, reg, index);
        this->set_register_state(*dst, field->type);
    }

    return *dst;
}

ErrorOr<bytecode::Register> State::generate_index_access(
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
            return err(expr.value().span(), "Cannot index into type '{}'", type->str());
        }

        if (type->is_pointer()) {
            reg = *option;
        } else {
            // FIXME: Use extractvalue in the LLVM backend for arrays
            return err(expr.value().span(), "Indexing into array immediates is not yet supported");
        }
    } else {
        reg = result.value();
        type = this->type(reg)->get_reference_type();

        if (!type->is_array() && !type->is_pointer()) {
            return err(expr.span(), "Cannot index into type '{}'", type->str());
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
        emit<bytecode::GetMemberRef>(*dst, reg, idx);
        this->set_register_state(*dst, inner->get_reference_to(as_mutable));
    } else {
        emit<bytecode::GetMember>(*dst, reg, idx);
        this->set_register_state(*dst, inner);
    }

    return *dst;
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

ErrorOr<size_t> State::size_of(ast::Expr const& expr) {
    Symbol* symbol = nullptr;
    String name;

    switch (expr.kind()) {
        case ast::ExprKind::Identifier: {
            auto* ident = expr.as<ast::IdentifierExpr>();
            symbol = m_current_scope->resolve(ident->name());

            if (!symbol) {
                auto iterator = STR_TO_TYPE.find(ident->name());
                if (iterator == STR_TO_TYPE.end()) {
                    return err(expr.span(), "Unknown identifier '{}'", ident->name());
                }

                Type* type = this->get_type_from_builtin(iterator->second);
                return type->size();
            }

            break;
        }
        case ast::ExprKind::Path: {
            auto* p = expr.as<ast::PathExpr>();
            auto& path = p->path();

            auto scope = TRY(this->resolve_scope_path(expr.span(), path));
            symbol = scope->resolve(path.name());

            if (!symbol) {
                return err(expr.span(), "Unknown identifier '{}'", path.format());
            }

            break;
        }
        default:
            return err(expr.span(), "Expected an identifier");
    }

    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = symbol->as<Variable>();
            return variable->value_type()->size();
        }
        case Symbol::Function: {
            auto* function = symbol->as<Function>();
            return function->underlying_type()->size();
        }
        case Symbol::Struct: {
            auto* structure = symbol->as<Struct>();
            return structure->underlying_type()->size();
        }
        case Symbol::TypeAlias: {
            auto* alias = symbol->as<TypeAlias>();
            if (alias->is_generic()) {
                return err(expr.span(), "Cannot determine the size of a generic type alias");
            }

            return alias->underlying_type()->size();
        }
        default:
            return err(expr.span(), "Cannot determine the size of '{}'", symbol->name());
    }

    return {};
}

}
