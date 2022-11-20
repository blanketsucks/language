#include "types/tuple.h"
#include "visitor.h"
#include "llvm/IR/DerivedTypes.h"

#include <sstream>

TupleType::TupleType(std::vector<Type*> types) : Type(Type::Tuple, 0), types(types) {}

TupleType* TupleType::create(std::vector<Type*> types) {
    auto type = new TupleType(types);
    return type;
};

TupleType* TupleType::from_llvm_type(llvm::StructType* type) {
    std::vector<Type*> types;
    for (auto& ty : type->elements()) {
        types.push_back(Type::from_llvm_type(ty));
    }

    return TupleType::create(types);
}

llvm::Type* TupleType::to_llvm_type(Visitor& visitor) {
    return nullptr;
}

std::vector<Type*> TupleType::getElementTypes() {
    return this->types;
}

uint32_t TupleType::getHashFromTypes(std::vector<Type*> types) {
    uint32_t hash = 0;
    for (auto& type : types) {
        hash |= type->getValue();
    }

    return hash;
}

uint32_t TupleType::hash() {
    return TupleType::getHashFromTypes(this->types);
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