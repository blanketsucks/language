#pragma once

#include <quart/language/types.h>
#include <quart/common.h>

#include <llvm/IR/Value.h>
#include <llvm/ADT/Any.h>

#define EMPTY_VALUE quart::Value(nullptr, quart::Value::Empty)

namespace quart {

struct Value {
    enum Flags : u16 {
        None,
        Empty             = 1 << 0,
        Constant          = 1 << 1,
        EarlyFunctionCall = 1 << 2,
        StackAllocated    = 1 << 3,
        Aggregate         = 1 << 4,

        // Flags to indicate the value inside the `extra` field
        Struct            = 1 << 5,
        Function          = 1 << 6,
        Builtin           = 1 << 7,
        Scope             = 1 << 8,
    };

    llvm::Value* inner;
    quart::Type* type;

    llvm::Value* self;

    u16 flags;

    llvm::Any extra;

    Value(
        llvm::Value* value, 
        u16 flags = None,
        llvm::Any extra = llvm::Any(),
        llvm::Value* self = nullptr
    );

    Value(
        llvm::Value* value, 
        quart::Type* type, 
        u16 flags = None, 
        llvm::Any extra = llvm::Any(),
        llvm::Value* self = nullptr
    );

    operator llvm::Value*() const { return this->inner; }
    llvm::Value* operator->() const { return this->inner; }

    [[nodiscard]] bool is_reference() const;
    [[nodiscard]] bool is_mutable() const;
    [[nodiscard]] bool is_aggregate() const;
    [[nodiscard]] bool is_empty_value() const;

    // Helper function to cast the `extra` field to a specific type
    template<typename T> T as() const { return llvm::any_cast<T>(this->extra); }
    template<typename T> [[nodiscard]] bool isa() const { return llvm::any_isa<T>(this->extra); }
};

}