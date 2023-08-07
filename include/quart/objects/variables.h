#pragma once

#include <quart/lexer/location.h>

#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Instructions.h>

struct Variable {
    std::string name;

    llvm::Type* type;
    llvm::Value* value;
    llvm::Constant* constant;

    bool is_reference;
    bool is_immutable;
    bool is_stack_allocated;
    bool is_used;
    bool is_mutated;

    Span span;

    static Variable from_alloca(
        const std::string& name,
        llvm::AllocaInst* alloca,
        bool is_immutable = false,
        Span span = Span()
    );

    static Variable from_value(
        const std::string& name,
        llvm::Value* value,
        bool is_immutable = false,
        bool is_reference = false,
        bool is_stack_allocated = true,
        Span span = Span()
    );

    static Variable null();
    bool is_null();
};

struct Constant {
    std::string name;
    llvm::Type* type;

    llvm::Value* store;
    llvm::Constant* value;

    Span span;

    static Constant null();
    bool is_null();
};
