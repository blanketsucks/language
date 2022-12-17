#ifndef _OBJECTS_ENUMS_H
#define _OBJECTS_ENUMS_H

#include "lexer/tokens.h"

#include "llvm/IR/Value.h"
#include "llvm/IR/Type.h"
#include <string>

struct Scope;

struct Enum {
    std::string name;
    llvm::Type* type;

    Location start;
    Location end;

    Scope* scope;

    Enum(std::string name, llvm::Type* type);

    void add_field(std::string name, llvm::Constant* value);
    bool has_field(std::string name);
    llvm::Value* get_field(std::string name);
};


#endif