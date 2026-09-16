#include <quart/language/state.h>
#include <quart/parser/parser.h>
#include <quart/casting.h>

namespace quart {

using bytecode::Value;
using bytecode::Constant, bytecode::ConstantInt;

State::State() : m_constant_evaluator(*this), m_type_checker(*this) {
    m_context = Context::create();
    m_current_scope = m_global_scope = Scope::create({}, ScopeType::Global, nullptr);
}

void State::dump() const {}

void State::switch_to(bytecode::BasicBlock* block) {
    m_generator.switch_to(block);
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

    if (symbol->type() == Symbol::Module) {
        scope = cast_unchecked<Module>(symbol)->scope();
    } else {
        scope = cast_unchecked<Struct>(symbol)->scope();
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

ErrorOr<Symbol*> State::access_symbol(Span span, const Path& path) {
    auto scope = TRY(this->resolve_scope_path(span, path));
    auto* symbol = scope->resolve(path.name());

    if (!symbol) {
        return err(span, "Unknown identifier '{}'", path.format());
    }

    if (!symbol->is_public() && symbol->module() != this->module()) {
        return err(span, "Cannot access private symbol '{}'", path.format());
    }

    return symbol;
}

ErrorOr<Value*> State::resolve_reference(Scope& scope, Span span, const String& name, ReferenceAccess access) {
    auto* symbol = scope.resolve(name);
    if (!symbol) {
        return err(span, "Unknown identifier '{}'", name);
    }

    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = cast_unchecked<Variable>(symbol);
            bool is_mutable_access = has_flag(access, ReferenceAccess::Mutable);
    
            if (!variable->is_mutable() && is_mutable_access) {
                return err(span, "Cannot take a mutable reference to an immutable variable");
            }

            Type* type = variable->value_type();
            if (has_flag(access, ReferenceAccess::PreserveOriginalMutability)) {
                type = type->get_reference_to(variable->is_mutable());
            } else {
                type = type->get_reference_to(is_mutable_access);
            }

            if (variable->flags() & Variable::Global) {
                return emit<bytecode::GetGlobalRef>(type, variable->index());
            } else {
                return emit<bytecode::GetLocalRef>(type, variable->index());
            }
        }
        default:
            return err(span, "Invalid reference");
    }
}

ErrorOr<Value*> State::resolve_reference(ast::Expr const& expr, ReferenceAccess access, bool use_default_case) {
    using ast::ExprKind;
    
    switch (expr.kind()) {
        case ExprKind::Identifier: {
            auto* ident = cast_unchecked<ast::IdentifierExpr>(expr);
            return this->resolve_reference(*m_current_scope, expr.span(), ident->name(), access);
        }
        case ExprKind::Path: {
            auto& path = cast_unchecked<ast::PathExpr>(expr)->path();
            auto scope = TRY(this->resolve_scope_path(expr.span(), path));

            return this->resolve_reference(*scope, expr.span(), path.name(), access);
        }
        case ExprKind::Attribute: {
            auto* attribute = cast_unchecked<ast::AttributeExpr>(expr);
            return TRY(this->generate_attribute_access(*attribute, access));
        }
        case ExprKind::Index: {
            auto index = cast_unchecked<ast::IndexExpr>(expr);
            return TRY(this->generate_index_access(*index, access));
        }
        default: {
            if (!use_default_case) {
                return err(expr.span(), "Invalid reference");
            }

            auto option = TRY(expr.generate(*this));
            if (!option.has_value()) {
                return err(expr.span(), "Expected an expression");
            }

            Value* value = *option;
            Type* type = value->type();
    
            if (!type->is_reference()) {
                Value* alloca = emit<bytecode::Alloca>(type);
                emit<bytecode::Write>(alloca, value);

                return alloca;
            }

            bool is_mutable_access = has_flag(access, ReferenceAccess::Mutable);
            if (is_mutable_access && !type->is_mutable()) {
                return err(expr.span(), "Cannot assign an immutable reference to a mutable reference");
            }

            return value;
        }
    }

    return {};
}

ErrorOr<Symbol*> State::resolve_symbol(ast::Expr const& expr) {
    using ast::ExprKind;
    
    switch (expr.kind()) {
        case ExprKind::Identifier: {
            auto* identifier = cast_unchecked<ast::IdentifierExpr>(expr);
            auto* symbol = m_current_scope->resolve(identifier->name());

            if (!symbol) {
                return err(expr.span(), "Unknown identifier '{}'", identifier->name());
            }

            return symbol;
        }
        case ExprKind::Path: {
            auto& path = cast_unchecked<ast::PathExpr>(expr)->path();
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
    if (auto* structure = cast<Struct>(symbol)) {
        return structure;
    }

    return err(expr.span(), "'{}' does not name a struct", symbol->name());
}

ErrorOr<Value*> State::type_check_and_cast(Span span, Value* value, Type* target, StringView error_message) {
    Type* type = value->type();
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

    return emit<bytecode::Cast>(value, target);
}

ErrorOr<Value*> State::generate_attribute_access(ast::AttributeExpr const& expr, ReferenceAccess access) {
    ast::Expr const& parent = expr.parent();
    auto result = this->resolve_reference(
        parent, 
        ReferenceAccess::Member | ReferenceAccess::PreserveOriginalMutability,
        false
    );

    Type* value_type = nullptr;
    Type* type = nullptr;

    bool is_mutable = false;
    Value* self = nullptr;

    if (result.is_err()) {
        auto& error = result.error();
        if (error.type() == ErrorType::MutabilityMismatch) {
            return error;
        }

        auto option = TRY(parent.generate(*this));
        if (!option.has_value()) {
            return err(parent.span(), "Expected an expression");
        }

        Value* value = option.value();
        type = value->type();

        if (!type->is_pointer() && !type->is_reference()) {
            value_type = type;

            self = emit<bytecode::Alloca>(type);
            emit<bytecode::Write>(self, value);
        } else {
            self = value;
            value_type = type->underlying_type();

            if (isa<bytecode::Alloca>(self)) {
                is_mutable = true;
            } else {
                is_mutable = type->is_mutable();
            }
        }
    } else {
        Value* value = result.value();

        type = value->type();
        is_mutable = type->is_mutable();

        type = type->get_reference_type();
        if (type->is_pointer() || type->is_reference()) {
            value_type = type->underlying_type();
            self = emit<bytecode::Read>(value);
        } else {
            value_type = type;
            self = value;
        }
    }

    Struct* structure = nullptr;
    if (isa<StructType>(value_type)) {
        structure = cast_unchecked<StructType>(value_type)->decl();
    }

    RefPtr<Scope> scope = nullptr;
    if (!structure) {
        if (type->is_underlying_type_of(quart::TypeKind::Function)) {
            type = type->underlying_type();
        }

        if (!this->has_impl(type)) {
            for (auto& impl : m_generic_impls) {
                scope = TRY(impl->make(*this, type));
                if (scope) {
                    break;
                }
            }

            if (!scope) {
                return err(parent.span(), "Cannot access attributes of type '{}'", type->str());
            }
        } else {
            auto& impl = *m_impls[type];
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

        auto& parameter = method->parameters().front();
        if (parameter.is_mutable() && !is_mutable) {
            return err(parent.span(), "Method '{}' requires a mutable reference to self but self is immutable", method->name());
        }

        this->inject_self(self);
        return method;
    }

    if (!structure) {
        return err(parent.span(), "Type '{}' has no attribute named '{}'", value_type->str(), attr);
    }

    auto* field = structure->find(attr);
    if (!field) {
        return err(expr.span(), "Unknown attribute '{}' for struct '{}'", attr, structure->name());
    } else if (!field->is_public() && m_current_struct != structure && structure->module() != m_current_module) {
        return err(expr.span(), "Cannot access private field '{}'", field->name);
    }

    auto* index = ConstantInt::get(context(), i32(), field->index);
    if (has_flag(access, ReferenceAccess::Member)) {
        return emit<bytecode::GetMemberRef>(field->type, self, index);
    } else {
        return emit<bytecode::GetMember>(field->type, self, index);
    }
}

ErrorOr<Value*> State::generate_index_access(ast::IndexExpr const& expr, ReferenceAccess access) {
    auto result = this->resolve_reference(expr.value(), ReferenceAccess::Member, false);

    Value* value = nullptr;
    Type* type = nullptr;

    bool deref = false;

    if (result.is_err()) {
        auto option = TRY(expr.value().generate(*this));
        if (!option.has_value()) {
            return err(expr.value().span(), "Expected an expression");
        }

        value = *option;
        type = value->type();

        if (!type->is_array() && !type->is_pointer()) {
            return err(expr.value().span(), "Cannot index into type '{}'", type->str());
        }

        if (!type->is_pointer()) {
            // FIXME: Use extractvalue in the LLVM backend for arrays
            return err(expr.value().span(), "Indexing into array immediates is not yet supported");
        }
    } else {
        value = result.value();
        type = value->type()->get_reference_type();

        if (!type->is_array() && !type->is_pointer() && !type->is_tuple()) {
            return err(expr.span(), "Cannot index into type '{}'", type->str());
        }

        deref = true;
    }

    if (type->is_tuple()) {
        if (!isa<ast::IntegerExpr>(expr.index())) {
            return err(expr.index().span(), "Array size must be an integer");
        }

        u64 index = cast_unchecked<ast::IntegerExpr>(expr.index())->value();
        if (index >= type->get_tuple_size()) {
            return err(expr.index().span(), "Index {} out of bounds for tuple of size {}", index, type->get_tuple_size());
        }

        Constant* idx = ConstantInt::get(context(), i32(), index);

        Type* inner = type->get_tuple_element(index);
        if (has_flag(access, ReferenceAccess::Member)) {
            return emit<bytecode::GetMemberRef>(inner, value, idx);
        } else {
            return emit<bytecode::GetMember>(inner, value, idx);
        }
    }

    Type* inner = nullptr;
    if (type->is_array()) {
        inner = type->get_array_element_type();
    } else if (type->is_pointer() && deref) {
        value = emit<bytecode::Read>(value);
        inner = type->get_pointee_type();
    } else {
        inner = type->get_pointee_type();
    }

    auto option = TRY(expr.index().generate(*this));
    if (!option.has_value()) {
        return err(expr.index().span(), "Expected an expression");
    }

    Value* index = *option;
    if (!index->type()->is_int()) {
        return err(expr.index().span(), "Expected an integer");
    }

    if (has_flag(access, ReferenceAccess::Member)) {
        inner = inner->get_reference_to(has_flag(access, ReferenceAccess::Mutable));
        return emit<bytecode::GetMemberRef>(inner, value, index);
    } else {
        return emit<bytecode::GetMember>(inner, value, index);
    }
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
            auto* ident = cast_unchecked<ast::IdentifierExpr>(expr);
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
            auto* p = cast_unchecked<ast::PathExpr>(expr);
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
            auto* variable = cast_unchecked<Variable>(symbol);
            return variable->value_type()->size();
        }
        case Symbol::Function: {
            auto* function = cast_unchecked<Function>(symbol);
            return function->underlying_type()->size();
        }
        case Symbol::Struct: {
            auto* structure = cast_unchecked<Struct>(symbol);
            return structure->underlying_type()->size();
        }
        case Symbol::TypeAlias: {
            auto* alias = cast_unchecked<TypeAlias>(symbol);
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

HashMap<String, Type*> State::get_struct_generic_impl_arguments(Struct* structure, TraitType* type) const {
    HashMap<String, Type*> arguments;

    auto trait = this->get_trait(type);
    for (auto& impl : structure->impls()) {
        if (!trait->has_scope(impl)) {
            continue;
        }

        auto scope = trait->resolve_scope(impl);
        for (auto& [name, span] : trait->generic_parameters()) {
            auto* alias = scope->resolve<TypeAlias>(name);
            arguments.insert({ name, alias->underlying_type() });
        }

        break;
    }

    return arguments;
}

}
