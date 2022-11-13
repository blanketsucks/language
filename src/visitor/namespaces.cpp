#include "objects.h"
#include "visitor.h"

Value Visitor::visit(ast::NamespaceExpr* expr) {
    utils::Shared<Namespace> ns = nullptr;
    while (!expr->parents.empty()) {
        std::string name = expr->parents.front();
        expr->parents.pop_front();

        ns = this->scope->namespaces[name];
        if (!ns) {
            ns = utils::make_shared<Namespace>(name, this->format_name(name));

            this->scope->namespaces[name] = ns;
            ns->scope = this->create_scope(name, ScopeType::Namespace);
        } else {
            this->scope = ns->scope;
        }

        this->current_namespace = ns;
    }

    if (this->scope->namespaces.find(expr->name) != this->scope->namespaces.end()) {
        ns = this->scope->namespaces[expr->name];
        this->scope = ns->scope;
    } else {
        ns =  utils::make_shared<Namespace>(name, this->format_name(expr->name));
        
        ns->start = expr->start;
        ns->end = expr->end;

        this->scope->namespaces[expr->name] = ns;
        ns->scope = this->create_scope(name, ScopeType::Namespace);
    }

    this->current_namespace = ns;
    for (auto& member : expr->members) {
        member->accept(*this);
    }

    this->current_namespace = nullptr;
    this->scope->exit(this);

    return nullptr;
}

Value Visitor::visit(ast::NamespaceAttributeExpr* expr) {
    Value value = expr->parent->accept(*this);
    if (!value.namespace_ && !value.structure && !value.enumeration && !value.module) {
        ERROR(expr->start, "Expected a namespace, struct, enum or module");
    }

    if (value.enumeration) {
        if (value.enumeration->has_field(expr->attribute)) {
            return value.enumeration->get_field(expr->attribute);
        }

        std::string name = value.enumeration->name;
        ERROR(expr->start, "Field '{0}' does not exist in enum '{1}'", expr->attribute, name);
    }

    Scope* scope;
    if (value.namespace_) {
        scope = value.namespace_->scope;
    } else if (value.structure) {
        scope = value.structure->scope;
    } else {
        scope = value.module->scope;
    }

    if (scope->structs.find(expr->attribute) != scope->structs.end()) {
        return Value::with_struct(scope->structs[expr->attribute]);
    } else if (scope->functions.find(expr->attribute) != scope->functions.end()) {
        auto func = scope->functions[expr->attribute];
        func->used = true;

        return Value::with_function(func);
    } else if (scope->namespaces.find(expr->attribute) != scope->namespaces.end()) {
        return Value::with_namespace(scope->namespaces[expr->attribute]);
    } else if (scope->constants.find(expr->attribute) != scope->constants.end()) {
        return Value(this->load(scope->constants[expr->attribute]), true);
    } else if (scope->enums.find(expr->attribute) != scope->enums.end()) {
        return Value::with_enum(scope->enums[expr->attribute]);
    } else if (scope->modules.find(expr->attribute) != scope->modules.end()) {
        return Value::with_module(scope->modules[expr->attribute]);
    } else {
        ERROR(expr->start, "Member '{0}' does not exist in named scope '{1}'", expr->attribute, scope->name);
    }
}

Value Visitor::visit(ast::UsingExpr* expr) {
    Value parent = expr->parent->accept(*this);
    if (!parent.namespace_ && !parent.module) {
        ERROR(expr->start, "Expected a namespace or module");
    }


    Scope* scope = nullptr;
    if (parent.namespace_) {
        scope = parent.namespace_->scope;
    } else {
        scope = parent.module->scope;
    }

    for (auto member : expr->members) {
        if (scope->structs.find(member) != scope->structs.end()) {
            this->scope->structs[member] = scope->structs[member];
        } else if (scope->functions.find(member) != scope->functions.end()) {
            this->scope->functions[member] = scope->functions[member];
        } else if (scope->namespaces.find(member) != scope->namespaces.end()) {
            this->scope->namespaces[member] = scope->namespaces[member];
        } else {
            ERROR(expr->start, "Member '{0}' does not exist in namespace '{1}'", member, scope->name);
        }
    }

    return nullptr;
}