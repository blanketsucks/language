#include "types.h"

#include <iostream>

Type::Type(Type::TypeValue value, int size) : value(value), size(size) {}

Type Type::from_llvm_type(llvm::Type* type) {
    if (type->isIntegerTy(16)) {
        return ShortType;
    } else if (type->isIntegerTy(32)) {
        return IntegerType;
    } else if (type->isIntegerTy(64)) {
        return LongType;
    } else if (type->isDoubleTy()) {
        return DoubleType;
    } else if (type->isFloatTy()) {
        return FloatType;
    } else {
        return VoidType;
    }
}

llvm::Type* Type::to_llvm_type(llvm::LLVMContext& context) {
    switch (this->value) {
        case Short:
            return llvm::Type::getInt16Ty(context);
        case Integer:
            return llvm::Type::getInt32Ty(context);
        case Double:
            return llvm::Type::getDoubleTy(context);
        case Float:
            return llvm::Type::getFloatTy(context);
        case String:
            return llvm::Type::getInt8PtrTy(context);
        case Boolean:
            return llvm::Type::getInt1Ty(context);
        default:
            return llvm::Type::getVoidTy(context);
    }
}

std::string Type::to_str() {
    switch (this->value) {
        case Short:
            return "short";
        case Integer:
            return "int";
        case Long:
            return "long";
        case Double:
            return "double";
        case Float:
            return "float";
        case String:
            return "str"; 
        case Boolean:
            return "bool";
        case Void:
            return "void";
        default:
            return "";
    }
}

bool Type::is_compatible(Type other) {
    if (this->value == other.value) {
        return true;
    } else if (this->is_numeric() && other.is_numeric()) {
        if (this->is_int() && other.is_floating_point()) {
            return false;
        }
        
        // TODO: Check signedness and possibly the size
        return true;
    } else if (this->is_string() && other.is_string() || this->is_void() && other.is_void()) {
        return true;
    } else {
        return false;
    }
}

bool Type::is_compatible(llvm::Type* type) {
    return this->is_compatible(Type::from_llvm_type(type));
}
