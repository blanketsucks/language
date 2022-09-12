#include "visitor.h"

Value Visitor::visit(ast::EnumExpr* expr) {
    llvm::Type* type = expr->type->to_llvm_type(this->context);
    Enum* enumeration = new Enum(expr->name, type);

    if (expr->type->isInteger()) {
        int value = 0;
        for (auto& field : expr->fields) {
            if (field.value) {
                if (!field.value->is_constant()) {
                    ERROR(field.value->start, "Enum fields must be constant values");
                }

                if (field.value->kind() != ast::ExprKind::Integer) {
                    ERROR(field.value->start, "Expected an integer");
                }

                int val = field.value->cast<ast::IntegerExpr>()->value;
                value = val;
            }

            llvm::Constant* constant = this->builder->getInt32(value);
            enumeration->add_field(field.name, constant);
        }

        this->enums[expr->name] = enumeration;
        return nullptr;
    }

    for (auto& field : expr->fields) {
        if (!field.value) {
            ERROR(expr->start, "Expected a value");
        }

        Value value = field.value->accept(*this);
        if (!value.is_constant) {
            ERROR(field.value->start, "Expected a constant value");
        }

        llvm::Constant* constant = llvm::cast<llvm::Constant>(value.unwrap(field.value->start));
        enumeration->add_field(field.name, constant);
    }

    this->enums[expr->name] = enumeration;
    return nullptr;
}