#pragma once

#include <quart/lexer/location.h>
#include <quart/language/types.h>
#include <quart/common.h>

#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Instructions.h>

namespace quart {

struct Variable {
    enum Flags : u8 {
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

    u8 flags;

    Span span;

    static Variable from_alloca(
        const std::string& name,
        llvm::AllocaInst* alloca,
        quart::Type* type,
        u8 flags,
        const Span& span
    );

    static Variable from_value(
        const std::string& name,
        llvm::Value* value,
        quart::Type* type,
        u8 flags,
        const Span& span
    );

    bool is_used() const { return this->flags & Variable::Used; }
    bool is_mutated() const { return this->flags & Variable::Mutated; }
    bool is_mutable() const { return this->flags & Variable::Mutable; }

    bool can_ignore_usage() const { return this->name[0] == '_'; }
};

struct Constant {
    std::string name;
    quart::Type* type;

    llvm::Value* store;
    llvm::Constant* value;

    Span span;
};

}