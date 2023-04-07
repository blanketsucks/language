#ifndef _OBJECTS_TYPES_H
#define _OBJECTS_TYPES_H

#include "llvm/IR/Type.h"

struct Struct;

struct Type {
    llvm::Type* value;

    bool is_reference;
    bool is_immutable;

    Type();
    Type(llvm::Type* type, bool is_reference = false, bool is_immutable = true);

    static Type null();

    bool is_null();

    llvm::Type* operator->();
    llvm::Type* operator*();

    operator llvm::Type*();
};

#endif