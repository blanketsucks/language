#include "types/array.h"

#include "utils.h"

#include <iostream>

ArrayType::ArrayType(size_t length, Type* element) : Type(Type::Array, length * element->getSize()), element(element), length(length) {}

ArrayType::~ArrayType() {
    // delete this->element;
}

ArrayType* ArrayType::create(size_t length, Type* element) {
    return new ArrayType(length, element);
}

ArrayType* ArrayType::from_llvm_type(llvm::ArrayType* type) {
    size_t length = type->getNumElements();
    Type* element = Type::from_llvm_type(type->getElementType());

    return ArrayType::create(length, element);
}

llvm::Type* ArrayType::to_llvm_type(llvm::LLVMContext& context) {
    llvm::Type* element = this->element->to_llvm_type(context);
    return llvm::ArrayType::get(element, this->length);
}

ArrayType* ArrayType::copy() {
    return ArrayType::create(this->length, this->element->copy());
}

std::string ArrayType::str() {
    return "[" + this->element->str() + "; " + std::to_string(this->length) + "]";
}

bool ArrayType::is_compatible(Type* other) {
    if (!other->isArray()) {
        if (other->isPointer()) {
            Type* ty = other->getPointerElementType();
            if (ty->is_compatible(this->element)) {
                return true;
            }
        } 

        return false;
    }

    ArrayType* array = other->cast<ArrayType>();
    if (this->length != array->getLength()) {
        return false;
    }

    Type* element = array->getElementType();
    return this->element->is_compatible(element);
}

bool ArrayType::is_compatible(llvm::Type* type) {
    if (!type->isArrayTy()) {
        return false;
    }

    return this->is_compatible(Type::from_llvm_type(type));
}
