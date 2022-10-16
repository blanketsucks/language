#include "visitor.h"

Value Visitor::visit(ast::NamespaceExpr* expr) {
    while (!expr->parents.empty()) {
        std::string name = expr->parents.front();
        expr->parents.pop_front();

        Namespace* ns;
        ns = this->scope->namespaces[name];
        if (!ns) {
            ns = new Namespace(name);

            this->scope->namespaces[name] = ns;
            ns->scope = this->create_scope(name, ScopeType::Namespace);
        } else {
            this->scope = ns->scope;
        }
    }

    std::string name = this->format_name(expr->name);
    if (this->scope->namespaces.find(name) != this->scope->namespaces.end()) {
        this->scope = this->scope->namespaces[name]->scope;
    } else {
        Namespace* ns = new Namespace(name);
        
        ns->start = expr->start;
        ns->end = expr->end;

        this->scope->namespaces[name] = ns;
        ns->scope = this->create_scope(name, ScopeType::Namespace);
    }

    for (auto& member : expr->members) {
        member->accept(*this);
    }

    this->current_namespace = nullptr;
    this->scope->exit(this);

    return nullptr;
}

Value Visitor::visit(ast::NamespaceAttributeExpr* expr) {
    Value value = expr->parent->accept(*this);
    if (!value.ns && !value.structure && !value.enumeration) {
        utils::error(expr->start, "Expected a namespace, struct or an enum");
    }

    if (value.structure) {
        if (value.structure->has_method(expr->attribute)) {
            Function* function = value.structure->methods[expr->attribute];
            function->used = true;

            return Value::with_function(function);
        }

        std::string name = value.structure->name;
        ERROR(expr->start, "Field '{0}' does not exist in struct '{1}'", expr->attribute, name);
    } else if (value.enumeration) {
        if (value.enumeration->has_field(expr->attribute)) {
            return value.enumeration->get_field(expr->attribute);
        }

        std::string name = value.enumeration->name;
        ERROR(expr->start, "Field '{0}' does not exist in enum '{1}'", expr->attribute, name);
    }

    Scope* scope = value.ns->scope;
    if (scope->structs.find(expr->attribute) != scope->structs.end()) {
        return Value::with_struct(scope->structs[expr->attribute]);
    } else if (scope->functions.find(expr->attribute) != scope->functions.end()) {
        Function* func = scope->functions[expr->attribute];
        func->used = true;

        return Value::with_function(func);
    } else if (scope->namespaces.find(expr->attribute) != scope->namespaces.end()) {
        return Value::with_namespace(scope->namespaces[expr->attribute]);
    } else if (scope->constants.find(expr->attribute) != scope->constants.end()) {
        return Value(scope->constants[expr->attribute], true);
    } else {
        ERROR(expr->start, "Member '{0}' does not exist in namespace '{1}'", expr->attribute, scope->name);
    }
}

Value Visitor::visit(ast::UsingExpr* expr) {
   Value parent = expr->parent->accept(*this);
   if (!parent.ns) {
        utils::error(expr->start, "Expected a namespace");
   }

   Scope* scope = parent.ns->scope;
   for (auto member : expr->members) {
       if (scope->structs.find(member) != scope->structs.end()) {
           this->structs[member] = scope->structs[member];
       } else if (scope->functions.find(member) != scope->functions.end()) {
           this->functions[member] = scope->functions[member];
       } else if (scope->namespaces.find(member) != scope->namespaces.end()) {
           this->namespaces[member] = scope->namespaces[member];
       } else {
           ERROR(expr->start, "Member '{0}' does not exist in namespace '{1}'", member, scope->name);
       }
   }

   return nullptr;
}