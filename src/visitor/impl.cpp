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

        Scope* scope = this->create_scope(name, ScopeType::Impl);
        Impl impl = {
            .name = name,
            .type = type,
            .scope = scope
        };

        this->self = type;
        this->current_impl = &impl;
    
        for (auto& function : expr->body) {
            function->accept(*this);
        }

        this->impls[type] = impl;
        scope->exit(this);

        this->current_impl = nullptr;
        this->self = nullptr;

        return nullptr;
    }

    if (!structure->scope->functions.empty()) {
        ERROR(expr->type->span, "An implementation already exists for struct '{0}'", structure->name);
    }

    this->scope = structure->scope;
    this->current_struct = structure;

    for (auto& function : expr->body) {
        function->accept(*this);
    }

    this->scope = this->scope->parent;
    this->current_struct = nullptr;

    return nullptr;

}