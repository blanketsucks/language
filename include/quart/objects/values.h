#ifndef _OBJECTS_VALUES_H
#define _OBJECTS_VALUES_H

#include <quart/utils/pointer.h>
#include <quart/lexer/location.h>

#include <quart/objects/functions.h>
#include <quart/objects/structs.h>
#include <quart/objects/enums.h>
#include <quart/objects/types.h>
#include <quart/objects/scopes.h>
#include <quart/builtins.h>

#include "llvm/IR/Value.h"

struct Value {
    llvm::Value* value;
    llvm::Value* self;

    bool is_constant;
    bool is_early_function_call;
    bool is_reference;
    bool is_immutable;
    bool is_stack_allocated;
    bool is_aggregate;

    utils::Ref<Function> function;
    utils::Ref<Struct> structure;
    Scope* scope;

    BuiltinFunction builtin;

    Type type = Type::null();

    Value(llvm::Value* value, bool is_constant = false, llvm::Value* self = nullptr);
    
    bool is_null();
    llvm::Value* unwrap(Span span);

    static Value null();

    static Value from_function(utils::Ref<Function> function, llvm::Value* self = nullptr);
    static Value from_struct(utils::Ref<Struct> structure);
    static Value from_scope(Scope* scope);
    static Value from_type(Type type, utils::Ref<Struct> structure = nullptr);
    static Value from_builtin(BuiltinFunction builtin);

    static Value as_early_function_call();
    static Value as_reference(
        llvm::Value* value, bool is_immutable = false, bool is_stack_allocated = true
    );
    static Value as_aggregate(llvm::Value* value);
};

#endif