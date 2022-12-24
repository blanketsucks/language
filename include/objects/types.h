#ifndef _OBJECTS_TYPES_H
#define _OBJECTS_TYPES_H

#include "llvm/IR/Type.h"

struct Type {
    llvm::Type* value;

    bool is_reference;

    Type() : value(nullptr), is_reference(false) {};
    Type(llvm::Type* type) : value(type), is_reference(false) {};
    Type(llvm::Type* type, bool is_reference) : value(type), is_reference(is_reference) {};

    static Type null() { return Type(nullptr); }
    bool is_null() { return this->value == nullptr; }

    llvm::Type* operator->() { return this->value; }
    llvm::Type* operator*() { return this->value; }
};

#endif