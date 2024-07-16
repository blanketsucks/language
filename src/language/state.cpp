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

ErrorOr<bytecode::Register> State::resolve_reference(Scope* scope, Span span, const String& name, bool is_mutable) {
    auto* symbol = scope->resolve(name);
    if (!symbol) {
        return err(span, "Unknown identifier '{0}'", name);
    }

    auto reg = this->allocate_register();
    switch (symbol->type()) {
        case Symbol::Variable: {
            auto* variable = symbol->as<Variable>();
            this->emit<bytecode::GetLocalRef>(reg, variable->local_index());

            // FIXME: Check if the variable has been marked as mutable and compare with `is_mutable`

            Type* type = variable->value_type();
            this->set_register_type(reg, type->get_reference_to(is_mutable));

            return reg;
        }
        default:
            return err(span, "Invalid reference");
    }
}

ErrorOr<bytecode::Register> State::resolve_reference(ast::Expr const& expr, bool is_mutable) {
    using ast::ExprKind;
    
    switch (expr.kind()) {
        case ExprKind::Identifier: {
            auto* ident = expr.as<ast::IdentifierExpr>();
            return this->resolve_reference(m_current_scope, expr.span(), ident->name(), is_mutable);
        }
        case ExprKind::Path: {
            auto& path = expr.as<ast::PathExpr>()->path();
            Scope* scope = TRY(this->resolve_scope_path(expr.span(), path));

            return this->resolve_reference(scope, expr.span(), path.name, is_mutable);
        }
        case ExprKind::Attribute: {
            break;
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

ErrorOr<bytecode::Operand> State::type_check_and_cast(Span span, bytecode::Operand operand, Type* target, StringView error_message) {
    Type* type = this->type(operand);
    if (!type->can_safely_cast_to(target)) {
        return err(span, error_message.data(), type->str(), target->str());
    } else if (type != target) {
        auto reg = this->allocate_register();
        this->emit<bytecode::Cast>(reg, operand, target);

        this->set_register_type(reg, target);
        operand = bytecode::Operand(reg);
    }

    return operand;
}

}
