#include "type.h"

#include <iostream>

#include "function.h"
#include "struct.h"
#include "pointer.h"
#include "array.h"

Type::Type(Type::Value value, int size) : value(value), size(size) {}


bool Type::operator==(Type::Value other) {
    return this->value == other;
} 

Type* Type::create(Type::Value value, int size) {
    return new Type(value, size);
}

Type* Type::fromLLVMType(llvm::Type* type) {
    if (type->isIntegerTy(8)) {
        return CharType;
    } else if (type->isIntegerTy(16)) {
        return ShortType;
    } else if (type->isIntegerTy(32)) {
        return IntegerType;
    } else if (type->isIntegerTy(LONG_SIZE)) {
        return LongType;
    } else if (type->isIntegerTy(64)) {
        return LongLongType;
    } else if (type->isDoubleTy()) {
        return DoubleType;
    } else if (type->isFloatTy()) {
        return FloatType;
    } else if (type->isStructTy()) {
        return StructType::fromLLVMType(llvm::cast<llvm::StructType>(type));
    } else if (type->isFunctionTy()) {
        return FunctionType::fromLLVMType(llvm::cast<llvm::FunctionType>(type));
    } else if (type->isPointerTy()) {
        if (type->getPointerElementType()->isFunctionTy()) {
            return FunctionType::fromLLVMType(llvm::cast<llvm::FunctionType>(type->getPointerElementType()));
        }

        return PointerType::fromLLVMType(type);
    } else if (type->isArrayTy()) {
        return ArrayType::fromLLVMType(llvm::cast<llvm::ArrayType>(type));
    } else {
        return VoidType;
    }
}

llvm::Type* Type::toLLVMType(llvm::LLVMContext& context) {
    switch (this->value) {
        case Type::Short:
            return llvm::Type::getInt16Ty(context);
        case Type::Integer:
            return llvm::Type::getInt32Ty(context);
        case Type::Long:
            if (LONG_SIZE == 32) {
                return llvm::Type::getInt32Ty(context);
            } else {
                return llvm::Type::getInt64Ty(context);
            }
        case Type::LongLong:
            return llvm::Type::getInt64Ty(context);
        case Type::Double:
            return llvm::Type::getDoubleTy(context);
        case Type::Float:
            return llvm::Type::getFloatTy(context);
        case Type::Char:
            return llvm::Type::getInt8Ty(context);
        case Type::String:
            return llvm::Type::getInt8PtrTy(context);
        case Type::Boolean:
            return llvm::Type::getInt1Ty(context);
        default:
            return llvm::Type::getVoidTy(context);
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
        case Type::LongLong:
            return "longlong";
        case Type::Double:
            return "double";
        case Type::Float:
            return "float";
        case Type::String:
            return "str"; 
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
        if (this->isInt() && other->isFloationPoint()) {
            return false;
        }
        
        // TODO: Check signedness and possibly the size
        return true;
    } else if (
        (this->isString() && other->isString()) || 
        (this->isVoid() && other->isVoid()) || 
        (this->isArray() && other->isArray())
    ) {
        return true;
    } else {
        return false;
    }
}

bool Type::is_compatible(llvm::Type* type) {
    return this->is_compatible(Type::fromLLVMType(type));
}
