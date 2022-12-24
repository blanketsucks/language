#ifndef _OBJECTS_VARIABLES_H
#define _OBJECTS_VARIABLES_H

#include "lexer/tokens.h"

#include "llvm/IR/Value.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"

struct Variable {
    std::string name;

    llvm::Type* type;
    llvm::Value* value;
    llvm::Constant* constant;

    bool is_reference;
    bool is_immutable;
    bool is_stack_allocated;

    Location start;
    Location end;

    static Variable from_alloca(
        std::string name, 
        llvm::AllocaInst* alloca,
        bool is_immutable = false,
        Location start = Location(),
        Location end = Location()
    );

    static Variable from_value(
        std::string name,
        llvm::Value* value,
        bool is_immutable = false,
        bool is_reference = false,
        bool is_stack_allocated = true,
        Location start = Location(),
        Location end = Location()
    );

    static Variable null();
    bool is_null();
};

struct Constant {
    std::string name;
    llvm::Type* type;

    llvm::Value* store;
    llvm::Constant* value;

    Location start;
    Location end;

    static Constant null();
    bool is_null();
};

#endif