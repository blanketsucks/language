#include <quart/visitor.h>

Value Visitor::visit(ast::ImplExpr* expr) {
    Type type = expr->type->accept(*this);
    auto structure = this->get_struct(type);
    if (!structure) {
        if (type.is_reference) {
            ERROR(expr->type->span, "Cannot implement a reference type");
        }

        std::string name = this->get_type_name(type);
        if (!this->is_valid_sized_type(type) || type->isArrayTy() || this->is_tuple(type)) {
            ERROR(expr->type->span, "Cannot implement type '{0}'", name);
        }

        if (type->isPointerTy()) {
            llvm::Type* elem = type->getPointerElementType();
            if (!elem->isIntegerTy(8)) {
                ERROR(expr->type->span, "Cannot implement type '{0}'", name);
            }

            name = "str";
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