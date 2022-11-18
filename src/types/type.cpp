#include "types/type.h"
#include "visitor.h"

#include <iostream>

#include "types/function.h"
#include "types/struct.h"
#include "types/pointer.h"
#include "types/array.h"
#include "types/tuple.h"

Type::Type(Type::Value value, size_t size) : value(value), size(size) {}

bool Type::operator==(Type::Value other) {
    return this->value == other;
} 

Type* Type::create(Type::Value value, size_t size) {
    auto type = new Type(value, size);
    return type;
}

Type* Type::from_llvm_type(llvm::Type* type) {
    return nullptr;
}

llvm::Type* Type::to_llvm_type(Visitor& visitor) {
    switch (this->value) {
        case Type::Short:
            return visitor.builder->getInt16Ty();
        case Type::Integer:
            return visitor.builder->getInt32Ty();
        case Type::Long:
            return visitor.builder->getIntNTy(LONG_SIZE);
        case Type::Double:
            return visitor.builder->getDoubleTy();
        case Type::Float:
            return visitor.builder->getFloatTy();
        case Type::Char:
            return visitor.builder->getInt8Ty();
        case Type::Boolean:
            return visitor.builder->getInt1Ty();
        default:
            return visitor.builder->getVoidTy();
    }
}

Type* Type::getPointerTo() {
    return PointerType::create(this);
}

Type* Type::getPointerElementType() {
    PointerType* pointer = this->cast<PointerType>();
    return pointer->getElementType();
}

Type* Type::getArrayElementType() {
    ArrayType* array = this->cast<ArrayType>();
    return array->getElementType();
}

std::vector<Type*> Type::getTupleElementTypes() {
    TupleType* tuple = this->cast<TupleType>();
    return tuple->getElementTypes();
}

Type* Type::getContainedType() {
    if (this->isPointer()) {
        return this->getPointerElementType();
    } else if (this->isArray()) {
        return this->getArrayElementType();
    } else {
        return nullptr;
    }
}

Type* Type::copy() {
    return Type::create(this->value, this->size);
}

std::string Type::str() {
    switch (this->value) {
        case Type::Short:
            return "short";
        case Type::Integer:
            return "int";
        case Type::Long:
            return "long";
        case Type::Double:
            return "double";
        case Type::Float:
            return "float"; 
        case Type::Char:
            return "char";
        case Type::Boolean:
            return "bool";
        case Type::Array:
            return "array";
        case Type::Struct:
            return "struct";
        case Type::Function:
            return "function";
        case Type::Void:
            return "void";
        default:
            return "";
    }
}

bool Type::is_compatible(Type* other) {
    if (this->value == other->value) {
        return true;
    } else if (this->isNumeric() && other->isNumeric()) {
        if (this->isInt() && other->isFloatingPoint()) {
            return false;
        }
        
        // TODO: Check signedness and possibly the size
        return true;
    } else if (
        (this->isVoid() && other->isVoid()) || (this->isArray() && other->isArray())
    ) {
        return true;
    } else {
        return false;
    }
}

bool Type::is_compatible(llvm::Type* type) {
    return this->is_compatible(Type::from_llvm_type(type));
}
