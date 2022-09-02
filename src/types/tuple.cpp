#include "types/tuple.h"

#include <sstream>

TupleType::TupleType(std::vector<Type*> types) : types(types), Type(Type::Tuple, 0) {}

TupleType::~TupleType() {}

TupleType* TupleType::create(std::vector<Type*> types) {
    return new TupleType(types);
};

TupleType* TupleType::from_llvm_type(llvm::StructType* type) {
    std::vector<Type*> types;
    for (auto& ty : type->elements()) {
        types.push_back(Type::from_llvm_type(ty));
    }

    return TupleType::create(types);
}

llvm::StructType* TupleType::to_llvm_type(llvm::LLVMContext& context) {
    std::vector<llvm::Type*> types;
    for (auto& type : this->types) {
        types.push_back(type->to_llvm_type(context));
    }

    return llvm::StructType::create(types, "__tuple");
}

std::vector<Type*> TupleType::getElementTypes() {
    return this->types;
}

uint32_t TupleType::getHashFromTypes(std::vector<Type*> types) {
    uint32_t hash = 0;
    for (auto& type : types) {
        hash |= type->hash();
    }

    return hash;
}

uint32_t TupleType::hash() {
    return TupleType::getHashFromTypes(this->types);
}

bool TupleType::has(Type::Value value) {
    return bool(this->hash() & value);
} 

bool TupleType::has(Type* type) {
    return this->has(type->getValue());
}

std::string TupleType::str() {
    std::stringstream stream; stream << "(";
    for (auto type : this->types) {
        stream << type->str() << ", ";
    }

    stream.seekp(-2, stream.cur); stream << ")";
    return stream.str();   
}

bool TupleType::is_compatible(Type* other) {
    if (!other->isTuple()) {
        return false;
    }

    TupleType* tuple = other->cast<TupleType>();
    if (this->hash() != tuple->hash()) {
        return false;
    }

    return true;
}

bool TupleType::is_compatible(llvm::Type* type) {
    if (!type->isStructTy()) {
        return false;
    }

    llvm::StringRef name = type->getStructName();
    if (!name.startswith("__tuple")) {
        return false;
    }

    return this->is_compatible(Type::from_llvm_type(type));
}