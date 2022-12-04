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

    Scope* scope = nullptr;
    if (value.namespace_) {
        scope = value.namespace_->scope;
    } else if (value.structure) {
        scope = value.structure->scope;
    } else {
        scope = value.module->scope;
    }

    if (scope->has_constant(expr->attribute)) {
        return Value(this->load(scope->constants[expr->attribute].value), true);
    } else if (scope->has_function(expr->attribute)) {
        return Value::with_function(scope->get_function(expr->attribute));
    } else if (scope->has_struct(expr->attribute)) {
        return Value::with_struct(scope->get_struct(expr->attribute));
    } else if (scope->has_enum(expr->attribute)) {
        return Value::with_enum(scope->get_enum(expr->attribute));
    } else if (scope->has_module(expr->attribute)) {
        return Value::with_module(scope->get_module(expr->attribute));
    } else if (scope->has_namespace(expr->attribute)) {
        return Value::with_namespace(scope->get_namespace(expr->attribute));
    } else if (scope->has_type(expr->attribute)) {
        return Value::with_type(scope->get_type(expr->attribute).type);
    }

    ERROR(expr->start, "Member '{0}' does not exist in named scope '{1}'", expr->attribute, scope->name);
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
        } else if (scope->constants.find(member) != scope->constants.end()) {
            this->scope->constants[member] = scope->constants[member];
        } else if (scope->enums.find(member) != scope->enums.end()) {
            this->scope->enums[member] = scope->enums[member];
        } else if (scope->modules.find(member) != scope->modules.end()) {
            this->scope->modules[member] = scope->modules[member];
        } else {
            ERROR(expr->start, "Member '{0}' does not exist in namespace '{1}'", member, scope->name);
        }
    }

    return nullptr;
}