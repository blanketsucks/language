#pragma once

#include <quart/lexer/location.h>

#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>

#include <string>

struct Scope;

struct Enum {
    std::string name;
    llvm::Type* type;

    Scope* scope;

    Span span;

    Enum(const std::string& name, llvm::Type* type);

    void add_field(const std::string& name, llvm::Constant* value);
    bool has_field(const std::string& name);
    llvm::Value* get_field(const std::string& name);
};
