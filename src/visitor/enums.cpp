#include "visitor.h"

Value Visitor::visit(ast::EnumExpr* expr) {
    llvm::Type* type = nullptr;
    if (!expr->type) {
        type = this->builder->getInt64Ty();
    } else {
        type = expr->type->accept(*this).type.value;
    }
    
    auto enumeration = utils::make_ref<Enum>(expr->name, type);
    enumeration->span = expr->span;

    this->scope->enums[expr->name] = enumeration;
    enumeration->scope = this->create_scope(expr->name, ScopeType::Enum);

    if (type->isIntegerTy()) {
        int64_t counter = 0;
        for (auto& field : expr->fields) {
            llvm::Constant* constant = nullptr;
            if (field.value) {
                llvm::Value* value = field.value->accept(*this).unwrap(field.value->span);
                if (!llvm::isa<llvm::ConstantInt>(value)) {
                    ERROR(field.value->span, "Expected a constant integer");
                }

                llvm::ConstantInt* val = llvm::cast<llvm::ConstantInt>(value);
                if (val->getBitWidth() > type->getIntegerBitWidth()) {
                    val = static_cast<llvm::ConstantInt*>(
                        llvm::ConstantInt::get(type, val->getSExtValue(), true)
                    );
                }

                counter = val->getSExtValue();
                constant = val;
            } else {
                constant = llvm::ConstantInt::get(type, counter, true);
            }

            enumeration->add_field(field.name, constant);
            counter++;
        }

        this->scope->exit(this);
        return nullptr;
    }

    for (auto& field : expr->fields) {
        if (!field.value) {
            ERROR(expr->span, "Expected a value");
        }

        Value value = field.value->accept(*this);
        if (!value.is_constant) {
            ERROR(field.value->span, "Expected a constant value");
        }

        llvm::Constant* constant = llvm::cast<llvm::Constant>(value.unwrap(field.value->span));
        enumeration->add_field(field.name, constant);
    }

    this->scope->exit(this);
    return nullptr;
}