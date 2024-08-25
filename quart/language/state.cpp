#include <quart/language/state.h>

namespace quart {

State::State() {
    m_type_registry = TypeRegistry::create();
    m_current_scope = Scope::create({}, ScopeType::Global, nullptr);
}

bytecode::Register State::allocate_register() {
    bytecode::Register reg = m_generator.allocate_register();
    m_registers.push_back(nullptr);

    return reg;
}

void State::switch_to(bytecode::BasicBlock* block) {
    m_generator.switch_to(block);
}

void State::set_register_type(bytecode::Register reg, Type* type) {
    m_registers[reg.index()] = type;
}

Type* State::type(bytecode::Register reg) const {
    return m_registers[reg.index()];
}

Type* State::type(bytecode::Operand operand) const {
    if (operand.is_immediate()) {
        return operand.value_type();
    } else {
        return m_registers[operand.value()];
    }
}

ErrorOr<Scope*> State::resolve_scope_path(Span span, const Path& path) {
    Scope* scope = m_current_scope;
    for (auto& segment : path.segments) {
        Span segment_span = { span.start(), span.start() + segment.size() };
        auto* symbol = scope->resolve(segment);
        
        if (!symbol) {
            return err(segment_span, "namespace '{0}' not found", segment);
        }

        if (!symbol->is(Symbol::Module, Symbol::Struct)) {
            return err(segment_span, "'{0}' is not a valid namespace", segment);
        }

        if (symbol->is<Module>()) {
            scope = symbol->as<Module>()->scope();
        } else {
            scope = symbol->as<Struct>()->scope();
        }

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
            this->emit<bytecode::GetLocalRef>(reg, variable->local_index());

            if (!variable->is_mutable() && is_mutable) {
                return err(span, "Cannot take a mutable reference to an immutable variable");
            }

            Type* type = variable->value_type();
            this->set_register_type(reg, type->get_reference_to(is_mutable));

            return reg;
        }
        default:
            return err(span, "Invalid reference");
    }
}

ErrorOr<bytecode::Register> State::resolve_reference(ast::Expr const& expr, bool is_mutable, Optional<bytecode::Register> dst) {
    using ast::ExprKind;
    
    switch (expr.kind()) {
        case ExprKind::Identifier: {
            auto* ident = expr.as<ast::IdentifierExpr>();
            return this->resolve_reference(m_current_scope, expr.span(), ident->name(), is_mutable, dst);
        }
        case ExprKind::Path: {
            auto& path = expr.as<ast::PathExpr>()->path();
            Scope* scope = TRY(this->resolve_scope_path(expr.span(), path));

            return this->resolve_reference(scope, expr.span(), path.name, is_mutable, dst);
        }
        case ExprKind::Attribute: {
            auto* attribute = expr.as<ast::AttributeExpr>();
            auto operand = TRY(this->generate_attribute_access(*attribute, true, dst, is_mutable));

            ASSERT(operand.is_register());
            return bytecode::Register(operand.value());
        }
        case ExprKind::Index: {
            break;
        }
        default:
            auto option = TRY(expr.generate(*this, {}));
            if (!option.has_value()) {
                return err(expr.span(), "Expected an expression");
            }

            Type* type = this->type(*option);
            if (!type->is_reference()) {
                return err(expr.span(), "Expected a reference type but got '{0}'", type->str());
            }

            auto reg = this->allocate_register();
            this->emit<bytecode::Move>(reg, *option);

            this->set_register_type(reg, type);
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

            auto* symbol = scope->resolve(path.name);
            if (!symbol) {
                return err(expr.span(), "Unknown identifier '{0}'", path.name);
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
    this->emit<bytecode::Cast>(reg, operand, target);

    this->set_register_type(reg, target);
    return bytecode::Operand(reg);
}

ErrorOr<bytecode::Operand> State::generate_attribute_access(ast::AttributeExpr const& expr, bool as_reference, Optional<bytecode::Register> dst, bool as_mutable) {
    ast::Expr const& parent = expr.parent();
    auto result = this->resolve_symbol(parent);

    if (result.is_err()) {
        // FIXME: 
        return result.error();
    }

    auto* symbol = result.value();
    if (!symbol->is<Variable>()) {
        return err(parent.span(), "Attribute access is only allowed on variables");
    }

    auto* variable = symbol->as<Variable>();

    Type* value_type = variable->value_type();
    bool is_pointer_type = false;

    if (value_type->is_pointer()) {
        value_type = value_type->get_pointee_type();
        is_pointer_type = true;
    }

    auto* structure = this->get_global_struct(value_type);
    if (!structure) {
        return err(parent.span(), "Cannot access attributes of type '{0}'", value_type->str());
    }

    auto* field = structure->find(expr.attribute());
    if (!field) {
        return err(expr.span(), "Unknown attribute '{0}' for struct '{1}'", expr.attribute(), structure->name());
    }

    if (as_reference && as_mutable && !variable->is_mutable()) {
        return err(parent.span(), "Cannot take a mutable reference to an immutable variable");
    }

    auto reg = this->allocate_register();
    this->set_register_type(reg, variable->value_type()->get_pointer_to());

    this->emit<bytecode::GetLocalRef>(reg, variable->local_index());
    if (is_pointer_type) {
        this->emit<bytecode::Read>(reg, reg);
        this->set_register_type(reg, variable->value_type());
    }

    if (!dst.has_value()) {
        dst = this->allocate_register();
    }

    if (as_reference) {
        this->emit<bytecode::GetMemberRef>(*dst, bytecode::Operand(reg), field->index);
        this->set_register_type(*dst, field->type->get_reference_to(as_mutable));
    } else {
        this->emit<bytecode::GetMember>(*dst, bytecode::Operand(reg), field->index);
        this->set_register_type(*dst, field->type);
    }

    return bytecode::Operand(*dst);
}

}
