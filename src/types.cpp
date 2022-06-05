#include "types.h"

#include <iostream>

Type::Type(Type::TypeValue value, int size) : value(value), size(size) {}

Type Type::from_llvm_type(llvm::Type* type) {
    if (type->isIntegerTy(16)) {
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
    } else {
        return VoidType;
    }
}

llvm::Type* Type::to_llvm_type(llvm::LLVMContext& context) {
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
        case Type::Byte:
            return llvm::Type::getInt8Ty(context);
        case Type::String:
            return llvm::Type::getInt8PtrTy(context);
        case Type::Boolean:
            return llvm::Type::getInt1Ty(context);
        case Type::Array:
            // Place-holder for now
            return llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0);
        default:
            return llvm::Type::getVoidTy(context);
    }
}

Type Type::copy() {
    return Type(this->value, this->size);
}

std::string Type::to_str() {
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
        case Type::String:
            return "str"; 
        case Type::Boolean:
            return "bool";
        case Type::Void:
            return "void";
    }

    return "";
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
    } else if (
        this->is_string() && other.is_string() || this->is_void() && other.is_void() || this->is_array() && other.is_array()
    ) {
        return true;
    } else {
        return false;
    }
}

bool Type::is_compatible(llvm::Type* type) {
    return this->is_compatible(Type::from_llvm_type(type));
}

// Structure Type

Struct::Struct(std::string name, std::vector<Type> fields) : name(name), fields(fields) {}

Struct Struct::from_llvm_type(llvm::StructType* type) {
    std::vector<Type> fields;
    for (auto field : type->elements()) {
        fields.push_back(Type::from_llvm_type(field));
    }

    return Struct(type->getName().str(), fields);
}

llvm::StructType* Struct::to_llvm_type(llvm::LLVMContext& context) {
    std::vector<llvm::Type*> types;
    for (auto& field : this->fields) {
        types.push_back(field.to_llvm_type(context));
    }

    return llvm::StructType::create(context, types);
}

bool Struct::is_compatible(Struct other) {
    if (this->fields.size() != other.fields.size()) {
        return false;
    }

    for (int i = 0; i < this->fields.size(); i++) {
        if (!this->fields[i].is_compatible(other.fields[i])) {
            return false;
        }
    }

    return true;
}

bool Struct::is_compatible(llvm::StructType* type) {
    return this->is_compatible(Struct::from_llvm_type(type));
}

