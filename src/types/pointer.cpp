#include "types/pointer.h"

#include "types/array.h"
#include "utils.h"

PointerType::PointerType(Type* type) : Type(Type::Pointer, LONG_SIZE), type(type) {}

PointerType::~PointerType() {
    // delete this->type;
}

PointerType* PointerType::create(Type* type) {
    return new PointerType(type);
}

PointerType* PointerType::fromLLVMType(llvm::Type* type) {
    return PointerType::create(Type::fromLLVMType(type->getPointerElementType()));
}

llvm::PointerType* PointerType::toLLVMType(llvm::LLVMContext& context) {
    return llvm::PointerType::get(this->type->toLLVMType(context), 0);
}

PointerType* PointerType::copy() {
    return PointerType::create(this->type->copy());
}

std::string PointerType::name() {
    return this->type->name();
}

std::string PointerType::str() {
    return this->type->str() + "*";
}

bool PointerType::is_compatible(Type* other) {
    // This is case of when you have a `char*` and a `[char; 69]`
    if (other->isArray()) {
        Type* ty = other->getArrayElementType();
        if (ty->isChar() && this->type->isChar()) {
            return true;
        }
    }

    if (this->type->isVoid() && other->isPointer()) {
        return true; // `void*` acts like an any type.
    }

    if (!other->isPointer()) {
        return false;
    }

    return this->type->is_compatible(other->getPointerElementType());
}

bool PointerType::is_compatible(llvm::Type* type) {
    if (!type->isPointerTy()) {
        return false;
    }

    return this->is_compatible(Type::fromLLVMType(type));
}