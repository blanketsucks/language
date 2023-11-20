#include <quart/visitor.h>

using namespace quart;

Value Visitor::visit(ast::PathExpr* expr) {
    Value value = expr->parent->accept(*this);
    Scope* scope = nullptr;

    if (value.flags & Value::Scope) {
        scope = value.as<Scope*>();
    } else if (value.flags & Value::Struct) {
        scope = value.as<Struct*>()->scope;
    } else {
        ERROR(expr->span, "Expected a namespace or module");
    }

    if (scope->has_constant(expr->name)) {
        Constant* constant = scope->get_constant(expr->name);
        return { constant->value, constant->type, Value::Constant };
    } else if (scope->has_struct(expr->name)) {
        auto structure = scope->get_struct(expr->name);
        return { nullptr, Value::Struct, structure.get() };
    } else if (scope->has_enum(expr->name)) {
        auto enumeration = scope->get_enum(expr->name);
        return { nullptr, Value::Scope, enumeration->scope };
    } else if (scope->has_function(expr->name)) {
        auto function = scope->get_function(expr->name);
        return { function->value, function->type, Value::Function | Value::Constant, function.get() };
    } else if (scope->has_module(expr->name)) {
        auto module = scope->get_module(expr->name);
        return { nullptr, Value::Scope, module->scope };
    }

    ERROR(expr->span, "Member '{0}' does not exist in namespace '{1}'", expr->name, scope->name);
}

Value Visitor::visit(ast::UsingExpr* expr) {
    Value value = expr->parent->accept(*this);
    if (!(value.flags & Value::Scope)) {
        ERROR(expr->span, "Expected a namespace or module");
    }

    auto scope = value.as<Scope*>();
    for (auto member : expr->members) {
        // TODO: Improve this somehow??
        if (scope->structs.find(member) != scope->structs.end()) {
            this->scope->structs[member] = scope->structs[member];
        } else if (scope->functions.find(member) != scope->functions.end()) {
            this->scope->functions[member] = scope->functions[member];
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