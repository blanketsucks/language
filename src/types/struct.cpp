#include "types/struct.h"

#include "utils.h"

StructType::StructType(std::string name, std::vector<Type*> fields) : Type(Type::Struct, 0), name(name), fields(fields) {}

StructType* StructType::create(std::string name, std::vector<Type*> fields) {
    auto type = new StructType(name, fields);
    Type::push(type);

    return type;
}

StructType* StructType::from_llvm_type(llvm::StructType* type) {
    std::vector<Type*> fields;
    for (auto field : type->elements()) {
        fields.push_back(Type::from_llvm_type(field));
    }

    return StructType::create(type->getName().str(), fields);
}

llvm::StructType* StructType::to_llvm_type(llvm::LLVMContext& context) {
    std::vector<llvm::Type*> types;
    for (auto& field : this->fields) {
        types.push_back(field->to_llvm_type(context));
    }

    return llvm::StructType::get(context, types);
}

StructType* StructType::copy() {
    return StructType::create(this->name, this->fields);
}

std::string StructType::str() {
    return this->name;
}

bool StructType::is_compatible(Type* other) {
    if (!other->isStruct()) {
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