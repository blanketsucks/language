#include "types.h"

#include <iostream>

Type::Type(Type::TypeValue value, int size) : value(value), size(size) {}

Type::Type(Type::TypeValue value, int size, std::vector<TypeVar> vars) : value(value), size(size) {
    this->vars = std::move(vars);
}

Type* Type::create(Type::TypeValue value, int size) {
    return new Type(value, size);
}

Type* Type::create(Type::TypeValue value, int size, std::vector<TypeVar> vars) {
    return new Type(value, size, std::move(vars));
}

Type* Type::from_llvm_type(llvm::Type* type) {
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
    } else if (type->isStructTy()) {
        return StructType::from_llvm_type(llvm::cast<llvm::StructType>(type));
    } else {
        return VoidType;
    }
}

llvm::Type* Type::to_llvm_type(llvm::LLVMContext& context) {
    llvm::Type* type;

    switch (this->value) {
        case Type::Short:
            type = llvm::Type::getInt16Ty(context);
            break;
        case Type::Integer:
            type = llvm::Type::getInt32Ty(context);
            break;
        case Type::Long:
            if (LONG_SIZE == 32) {
                type = llvm::Type::getInt32Ty(context);
            } else {
                type = llvm::Type::getInt64Ty(context);
            }

            break;
        case Type::LongLong:
            type = llvm::Type::getInt64Ty(context);
            break;
        case Type::Double:
            type = llvm::Type::getDoubleTy(context);
            break;
        case Type::Float:
            type = llvm::Type::getFloatTy(context);
            break;
        case Type::Byte:
            type = llvm::Type::getInt8Ty(context);
            break;
        case Type::String:
            type = llvm::Type::getInt8PtrTy(context);
            break;
        case Type::Boolean:
            type = llvm::Type::getInt1Ty(context);
            break;
        case Type::Array: {
            Type* element = llvm::any_cast<Type*>(this->vars[0].value);
            int size = llvm::any_cast<int>(this->vars[1].value);
            
            type = llvm::ArrayType::get(element->to_llvm_type(context), size);
            break;
        }
        default:
            type = llvm::Type::getVoidTy(context);
    }

    if (this->is_pointer) {
        type = type->getPointerTo();
    }

    return type;
}

Type* Type::copy() {
    return Type::create(this->value, this->size, this->vars);
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
        case Type::Array:
            return "array";
        case Type::Struct:
            return "struct";
        case Type::Void:
            return "void";
    }

    return "";
}

bool Type::is_compatible(Type* other) {
    if (this->value == other->value) {
        return true;
    } else if (this->is_numeric() && other->is_numeric()) {
        if (this->is_int() && other->is_floating_point()) {
            return false;
        }
        
        // TODO: Check signedness and possibly the size
        return true;
    } else if (
        this->is_string() && other->is_string() || this->is_void() && other->is_void() || this->is_array() && other->is_array()
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

StructType::StructType(std::string name, std::vector<Type*> fields) : Type(Type::Struct, 0), name(name), fields(fields) {}

StructType* StructType::create(std::string name, std::vector<Type*> fields) {
    return new StructType(name, fields);
}

StructType* StructType::from_llvm_type(llvm::StructType* type) {
    std::vector<Type*> fields;
    for (auto field : type->elements()) {
        fields.push_back(Type::from_llvm_type(field));
    }

    return StructType::create(type->getName().str(), fields);
}

llvm::Type* StructType::to_llvm_type(llvm::LLVMContext& context) {
    std::vector<llvm::Type*> types;
    for (auto& field : this->fields) {
        types.push_back(field->to_llvm_type(context));
    }

    return llvm::StructType::get(context, types);
}

bool StructType::is_compatible(Type* other) {
    if (!other->is_struct()) {
        return false;
    }

    // if (this->fields.size() != other->fields.size()) {
    //     return false;
    // }

    // for (int i = 0; i < this->fields.size(); i++) {
    //     if (!this->fields[i]->is_compatible(other->fields[i])) {
    //         return false;
    //     }
    // }

    return true;
}

bool StructType::is_compatible(llvm::Type* type) {
    if (!type->isStructTy()) {
        return false;
    }

    return this->is_compatible(StructType::from_llvm_type(llvm::cast<llvm::StructType>(type)));
}

