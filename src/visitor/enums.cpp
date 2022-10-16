#include "utils.h"
#include "visitor.h"

Value Visitor::visit(ast::EnumExpr* expr) {
    llvm::Type* type = expr->type->to_llvm_type(*this->context);
    Enum* enumeration = new Enum(expr->name, type);

    enumeration->start = expr->start;
    enumeration->end = expr->end;

    enumeration->scope = new Scope(expr->name, ScopeType::Enum, this->scope);
    this->scope->enums[expr->name] = enumeration;

    this->scope = enumeration->scope;
    if (type->isIntegerTy()) {
        int64_t counter = 0;
        for (auto& field : expr->fields) {
            llvm::Constant* constant = nullptr;
            if (field.value) {
                llvm::Value* value = field.value->accept(*this).unwrap(field.value->start);
                if (!llvm::isa<llvm::ConstantInt>(value)) {
                    utils::error(field.value->start, "Expected a constant integer");
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
            utils::error(expr->start, "Expected a value");
        }

        Value value = field.value->accept(*this);
        if (!value.is_constant) {
            utils::error(field.value->start, "Expected a constant value");
        }

        llvm::Constant* constant = llvm::cast<llvm::Constant>(value.unwrap(field.value->start));
        enumeration->add_field(field.name, constant);
    }

    this->scope->exit(this);
    return nullptr;
}