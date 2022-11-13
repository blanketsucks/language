#include "types/pointer.h"

#include "types/array.h"
#include "types/type.h"
#include "utils.h"

PointerType::PointerType(Type* type) : Type(Type::Pointer, LONG_SIZE), type(type) {}

PointerType* PointerType::create(Type* ty) {
    auto type = new PointerType(ty);
    Type::push(type);

    return type;
}

PointerType* PointerType::from_llvm_type(llvm::Type* type) {
    return PointerType::create(Type::from_llvm_type(type->getNonOpaquePointerElementType()));
}

llvm::PointerType* PointerType::to_llvm_type(Visitor& visitor) {
    return this->type->to_llvm_type(visitor)->getPointerTo();
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

    return this->is_compatible(Type::from_llvm_type(type));
}