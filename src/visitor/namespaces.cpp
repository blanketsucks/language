#include "visitor.h"

Value Visitor::visit(ast::NamespaceExpr* expr) {
    std::string name = this->format_name(expr->name);
    if (this->namespaces.find(name) != this->namespaces.end()) {
        this->current_namespace = this->namespaces[name];
    } else {
        Namespace* ns = new Namespace(name);

        if (this->current_namespace) {
            this->current_namespace->namespaces[expr->name] = ns;
        }

        this->namespaces[name] = ns;
        this->current_namespace = ns;
    }

    for (auto& member : expr->members) {
        member->accept(*this);
    }

    this->current_namespace = nullptr;
    return nullptr;
}

Value Visitor::visit(ast::NamespaceAttributeExpr* expr) {
    Value value = expr->parent->accept(*this);
    if (!value.ns && !value.structure) {
        ERROR(expr->start, "Expected a namespace or a struct");
    }

    if (value.structure) {
        if (value.structure->has_method(expr->attribute)) {
            Function* function = value.structure->methods[expr->attribute];
            function->used = true;

            return Value::with_function(function);
        }

        std::string name = value.structure->name;
        ERROR(expr->start, "Field '{s}' does not exist in struct '{s}'", expr->attribute, name);
    }

    Namespace* ns = value.ns;
    if (ns->structs.find(expr->attribute) != ns->structs.end()) {
        return Value::with_struct(ns->structs[expr->attribute]);
    } else if (ns->functions.find(expr->attribute) != ns->functions.end()) {
        Function* func = ns->functions[expr->attribute];
        func->used = true;

        return Value::with_function(func);
    } else if (ns->namespaces.find(expr->attribute) != ns->namespaces.end()) {
        return Value::with_namespace(ns->namespaces[expr->attribute]);
    } else if (ns->locals.find(expr->attribute) != ns->locals.end()) {
        return ns->locals[expr->attribute];
    } else {
        ERROR(expr->start, "Attribute '{s}' does not exist in namespace '{s}'", expr->attribute, ns->name); exit(1);
    }

    return nullptr;
}

Value Visitor::visit(ast::UsingExpr* expr) {
   Value parent = expr->parent->accept(*this);
   if (!parent.ns) {
        ERROR(expr->start, "Expected a namespace or a module");
    }

    Namespace* ns = parent.ns;
    for (auto member : expr->members) {
        if (ns->structs.find(member) != ns->structs.end()) {
            this->structs[member] = ns->structs[member];
        } else if (ns->functions.find(member) != ns->functions.end()) {
            this->functions[member] = ns->functions[member];
        } else if (ns->namespaces.find(member) != ns->namespaces.end()) {
            this->namespaces[member] = ns->namespaces[member];
        } else {
            ERROR(expr->start, "Member '{s}' does not exist in namespace '{s}'", member, ns->name);
        }
    }

    return nullptr;
}