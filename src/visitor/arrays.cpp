#include "visitor.h"

Value Visitor::visit(ast::ArrayExpr* expr) {
    std::vector<Value> elements;
    bool is_const = std::all_of(expr->elements.begin(), expr->elements.end(), [](auto& element) {
        return element->is_constant();
    });

    for (auto& element : expr->elements) {
        Value value = element->accept(*this);
        elements.push_back(value);
    }

    if (elements.size() == 0) {
        return llvm::Constant::getNullValue(llvm::ArrayType::get(llvm::Type::getInt32Ty(this->context), 0));
    }

    // The type of an array is determined from the first element.
    llvm::Type* etype = elements[0].type();
    for (size_t i = 1; i < elements.size(); i++) {
        if (elements[i].type() != etype) {
            ERROR(expr->start, "All elements of an array must be of the same type");
        }
    }

    llvm::Function* parent = this->builder->GetInsertBlock()->getParent();
    llvm::ArrayType* type = llvm::ArrayType::get(etype, elements.size());

    if (is_const) {
        std::vector<llvm::Constant*> constants;
        for (auto elem : elements) {
            constants.push_back((llvm::Constant*)elem.value);
        }

        return Value(llvm::ConstantArray::get(type, constants), true);
    }

    llvm::AllocaInst* array = this->create_alloca(parent, type);
    for (size_t i = 0; i < elements.size(); i++) {
        std::vector<llvm::Value*> indices = {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context), llvm::APInt(32, 0)),
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context), llvm::APInt(32, i))
        };

        llvm::Value* ptr = this->builder->CreateGEP(type, array, indices);
        this->builder->CreateStore(elements[i].unwrap(this, expr->start), ptr);
    }

    return array;
}


Value Visitor::visit(ast::ElementExpr* expr) {
    llvm::Value* value = expr->value->accept(*this).unwrap(this, expr->value->start);
    llvm::Type* type = value->getType();

    bool is_pointer = false;
    if (!type->isArrayTy()) {
        if (!type->isPointerTy()) {
            ERROR(expr->start, "Expected an array or a pointer");
        }

        type = type->getPointerElementType();
        is_pointer = true;
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(this, expr->index->start);
    if (is_pointer) {
        llvm::Type* element = type;
        if (type->isArrayTy()) {
            element = type->getArrayElementType();
        } 

        llvm::Value* ptr = this->builder->CreateGEP(type, value, index);
        return this->builder->CreateLoad(element, ptr);
    }
    
    return nullptr;
}

llvm::Value* Visitor::get_array_element(ast::ElementExpr* expr) {
    llvm::Value* parent = expr->value->accept(*this).unwrap(this, expr->start);
    llvm::Type* type = parent->getType();

    if (!type->isPointerTy()) {
        ERROR(expr->start, "Array access on non-array type");
    }

    type = type->getPointerElementType();
    if (!type->isArrayTy()) {
        ERROR(expr->start, "Array access on non-array type");
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(this, expr->start);
    if (!index->getType()->isIntegerTy()) {
        ERROR(expr->index->start, "Indicies must be integers");
    }

    return this->builder->CreateGEP(type, parent, index);
}