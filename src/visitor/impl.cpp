#include <quart/visitor.h>

using namespace quart;

Value Visitor::visit(ast::ImplExpr* expr) {
    quart::Type* type = expr->type->accept(*this);
    auto structure = this->get_struct_from_type(type);

    if (!structure) {
        if (type->is_reference()) {
            ERROR(expr->type->span, "Cannot implement a reference type");
        }

        std::string name = type->get_as_string();
        if (!type->is_sized_type()) {
            ERROR(expr->type->span, "Cannot implement type '{0}'", name);
        }

        Scope* scope = Scope::create(name, ScopeType::Impl, this->scope);
        
        Impl impl = {
            .name = name,
            .type = type,
            .scope = scope
        };

        this->self = type;
        this->current_impl = &impl;
    
        this->push_scope(scope);
        for (auto& function : expr->body) {
            function->accept(*this);
        }

        this->impls[type] = impl;
        this->pop_scope();

        this->current_impl = nullptr;
        this->self = nullptr;

        return nullptr;
    }

    if (!structure->scope->functions.empty()) {
        ERROR(expr->type->span, "An implementation already exists for struct '{0}'", structure->name);
    }

    this->push_scope(structure->scope);
    this->current_struct = structure;

    for (auto& function : expr->body) {
        function->accept(*this);
    }

    this->pop_scope();
    this->current_struct = nullptr;

    return nullptr;

}