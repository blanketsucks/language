#ifndef _OBJECTS_VALUES_H
#define _OBJECTS_VALUES_H

#include "utils/pointer.h"
#include "lexer/location.h"

#include "objects/functions.h"
#include "objects/structs.h"
#include "objects/modules.h"
#include "objects/namespaces.h"
#include "objects/enums.h"
#include "objects/types.h"
#include "builtins.h"

#include "llvm/IR/Value.h"

struct Value {
    llvm::Value* value;
    llvm::Value* self;

    bool is_constant;
    bool is_early_function_call;
    bool is_reference;
    bool is_immutable;
    bool is_stack_allocated;

    utils::Ref<Function> function;
    utils::Ref<Struct> structure;
    utils::Ref<Enum> enumeration;
    utils::Ref<Namespace> namespace_;
    utils::Ref<Module> module;

    BuiltinFunction builtin;

    Type type = Type::null();

    Value(llvm::Value* value, bool is_constant = false, llvm::Value* self = nullptr);
    
    bool is_null();
    llvm::Value* unwrap(Span span);

    static Value null();

    static Value from_function(utils::Ref<Function> function, llvm::Value* self = nullptr);
    static Value from_struct(utils::Ref<Struct> structure);
    static Value from_module(utils::Ref<Module> module);
    static Value from_namespace(utils::Ref<Namespace> namespace_);
    static Value from_enum(utils::Ref<Enum> enumeration);
    static Value from_type(Type type);
    static Value from_builtin(BuiltinFunction builtin);

    static Value as_early_function_call();
    static Value as_reference(
        llvm::Value* value, bool is_immutable = false, bool is_stack_allocated = true
    );
};

#endif