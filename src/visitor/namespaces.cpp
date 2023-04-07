#include <quart/visitor.h>

Value Visitor::visit(ast::NamespaceExpr* expr) {
    utils::Ref<Namespace> ns = nullptr;
    while (!expr->parents.empty()) {
        std::string name = expr->parents.front();
        expr->parents.pop_front();

        ns = this->scope->namespaces[name];
        if (!ns) {
            ns = utils::make_ref<Namespace>(name, this->format_name(name));

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
        ns =  utils::make_ref<Namespace>(name, this->format_name(expr->name));
        ns->span = expr->span;

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
    if (!value.scope && !value.structure) {
        ERROR(expr->span, "Expected a namespace, struct, enum or module");
    }


    Scope* scope = value.scope ? value.scope : value.structure->scope;
    if (scope->has_constant(expr->attribute)) {
        return Value(this->load(scope->constants[expr->attribute].value), true);
    } else if (scope->has_function(expr->attribute)) {
        return Value::from_function(scope->get_function(expr->attribute));
    } else if (scope->has_struct(expr->attribute)) {
        return Value::from_struct(scope->get_struct(expr->attribute));
    } else if (scope->has_enum(expr->attribute)) {
        auto& enumeration = scope->enums[expr->attribute];
        return Value::from_scope(enumeration->scope);
    } else if (scope->has_module(expr->attribute)) {
        auto& module = scope->modules[expr->attribute];
        return Value::from_scope(module->scope);
    } else if (scope->has_namespace(expr->attribute)) {
        auto& ns = scope->namespaces[expr->attribute];
        return Value::from_scope(ns->scope);
    } else if (scope->has_type(expr->attribute)) {
        return Value::from_type(scope->get_type(expr->attribute).type);
    }

    ERROR(expr->span, "Member '{0}' does not exist in namespace '{1}'", expr->attribute, scope->name);
}

Value Visitor::visit(ast::UsingExpr* expr) {
    Value parent = expr->parent->accept(*this);
    if (!parent.scope) {
        ERROR(expr->span, "Expected a namespace or module");
    }

    Scope* scope = parent.scope;
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
            ERROR(expr->span, "Member '{0}' does not exist in namespace '{1}'", member, scope->name);
        }
    }

    return nullptr;
}