#include "visitor.h"
#include "llvm/ADT/APInt.h"

#include <cstdio>

Value Visitor::visit(ast::ArrayExpr* expr) {
    std::vector<llvm::Value*> elements;
    bool is_const = true;

    for (auto& element : expr->elements) {
        Value value = element->accept(*this);
        is_const &= value.is_constant;

        elements.push_back(value.unwrap(element->start));
    }

    if (elements.size() == 0) {
        if (!this->ctx) {
            utils::note(expr->start, "The type of empty arrays defaults to [int; 0]");
             return llvm::ConstantAggregateZero::get(
                llvm::ArrayType::get(llvm::Type::getInt32Ty(*this->context), 0)
            );
        }

        if (!this->ctx->isArrayTy()) {
            ERROR(expr->start, "Expected an array type");
        }

        return llvm::ConstantAggregateZero::get(this->ctx);
    }

    // The type of the array is determined from the first element.
    llvm::Type* etype = elements[0]->getType();
    for (size_t i = 1; i < elements.size(); i++) {
        llvm::Value* element = elements[i];
        if (!this->is_compatible(element->getType(), etype)) {
            ERROR(expr->start, "All elements of an array must be of the same type");
        } else {
            elements[i] = this->cast(element, etype);
        }
    }

    llvm::ArrayType* type = llvm::ArrayType::get(etype, elements.size());
    if (is_const) {
        std::vector<llvm::Constant*> constants;
        for (auto element : elements) {
            constants.push_back(llvm::cast<llvm::Constant>(element));
        }
        
        return Value(llvm::ConstantArray::get(type, constants), true);
    }

    llvm::Value* array = llvm::ConstantAggregateZero::get(type);
    for (uint32_t i = 0; i < elements.size(); i++) {
        array = this->builder->CreateInsertValue(array, elements[i], {0, i});
    }

    return array;
}

Value Visitor::visit(ast::ElementExpr* expr) {
    bool is_const = false;
    Value val = expr->value->accept(*this); is_const = val.is_constant;

    llvm::Value* value = val.unwrap(expr->value->start);
    llvm::Type* type = value->getType();
    if (type->isStructTy()) {
        llvm::StringRef name = type->getStructName();
        if (!name.startswith("__tuple")) {
            ERROR(expr->value->start, "Expected a pointer, array or tuple");
        }

        llvm::Value* val = expr->index->accept(*this).unwrap(expr->index->start);
        if (!llvm::isa<llvm::ConstantInt>(val)) {
            ERROR(expr->index->start, "Tuple indices must be integer constants");
        }

        llvm::ConstantInt* index = llvm::cast<llvm::ConstantInt>(val);
        int64_t idx = index->getSExtValue();

        if (idx < 0) {
            ERROR(expr->index->start, "Tuple indices must be a positive integer");
        }

        uint32_t elements = type->getStructNumElements();
        if (idx > elements) {
            ERROR(expr->index->start, "Element index out of bounds");
        }

        if (is_const) {
            llvm::ConstantStruct* tuple = llvm::cast<llvm::ConstantStruct>(value);
            return Value(tuple->getAggregateElement(idx), true);
        }

        llvm::Value* ptr = this->builder->CreateStructGEP(type, value, idx);
        return this->builder->CreateLoad(type->getStructElementType(idx), ptr);
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(expr->index->start);
    if (!index->getType()->isIntegerTy()) {
        ERROR(expr->index->start, "Indicies must be integers");
    }

    if (is_const) {
        if (!llvm::isa<llvm::ConstantInt>(index)) {
            ERROR(expr->index->start, "Expected a constant integer as the array index");
        }

        llvm::ConstantInt* idx = llvm::cast<llvm::ConstantInt>(index);
        llvm::ConstantArray* array = llvm::cast<llvm::ConstantArray>(value);

        if (idx->getSExtValue() > int64_t(array->getType()->getArrayNumElements() - 1)) {
            ERROR(expr->index->start, "Element index out of bounds");
        }

        return Value(array->getAggregateElement(idx->getSExtValue()), true);
    }

    llvm::Type* element = nullptr;
    if (type->isArrayTy()) {
        element = type->getArrayElementType();
    } else {
        element = type->getNonOpaquePointerElementType();
    }

    llvm::Value* ptr = nullptr;
    if (!type->isArrayTy()) {
        ptr = this->builder->CreateGEP(element, value, index);
    } else {
        // ExtractValue doesn't take a Value so i'm kinda forced to do this.
        value = this->get_pointer_from_expr(expr->value.get()).first;
        ptr = this->builder->CreateGEP(type, value, {this->builder->getInt32(0), index});
    }
    
    return this->load(ptr, element);
}

void Visitor::store_array_element(ast::ElementExpr* expr, utils::Ref<ast::Expr> value) {
    llvm::Value* parent = this->get_pointer_from_expr(expr->value.get()).first;
    llvm::Type* type = parent->getType();

    if (!type->isPointerTy()) {
        ERROR(expr->value->start, "Value of type '{0}' does not support item assignment", type);
    }

    type = type->getNonOpaquePointerElementType();
    if (type->isStructTy()) {
        ERROR(expr->value->start, "Value of type '{0}' does not support item assignment", type);
    }

    llvm::Value* index = expr->index->accept(*this).unwrap(expr->start);
    if (!index->getType()->isIntegerTy()) {
        ERROR(expr->index->start, "Indices must be integers");
    }

    llvm::Value* element = value->accept(*this).unwrap(value->start);
    llvm::Value* ptr = nullptr;

    if (type->isArrayTy()) {
        std::vector<llvm::Value*> indicies = {this->builder->getInt32(0), index};
        ptr = this->builder->CreateGEP(type, parent, indicies);
    } else {
        ptr = this->builder->CreateGEP(type, parent, index);
    }

    this->builder->CreateStore(element, ptr);
}