#pragma once

#include <quart/lexer/location.h>
#include <quart/language/types.h>

#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Instructions.h>

namespace quart {

struct Variable {
    enum Flags : uint8_t {
        None,
        StackAllocated = 1 << 0,
        Reference      = 1 << 1,
        Mutable        = 1 << 2,
        Used           = 1 << 3,
        Mutated        = 1 << 4,
    };

    std::string name;
    quart::Type* type;

    llvm::Value* value;
    llvm::Constant* constant;

    uint8_t flags;

    Span span;

    static Variable from_alloca(
        const std::string& name,
        llvm::AllocaInst* alloca,
        quart::Type* type,
        uint8_t flags,
        const Span& span
    );

    static Variable from_value(
        const std::string& name,
        llvm::Value* value,
        quart::Type* type,
        uint8_t flags,
        const Span& span
    );
};

struct Constant {
    std::string name;
    quart::Type* type;

    llvm::Value* store;
    llvm::Constant* value;

    Span span;
};

}