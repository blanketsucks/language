#ifndef _OBJECTS_VALUES_H
#define _OBJECTS_VALUES_H

#include "utils/pointer.h"

#include "objects/functions.h"
#include "objects/structs.h"
#include "objects/modules.h"
#include "objects/namespaces.h"
#include "objects/enums.h"
#include "objects/types.h"

#include "llvm/IR/Value.h"

struct Value {
    llvm::Value* value;
    llvm::Value* self;

    bool is_constant;
    bool is_early_function_call;
    bool is_reference;
    bool is_immutable;
    bool is_stack_allocated;

    utils::Shared<Function> function;
    utils::Shared<Struct> structure;
    utils::Shared<Enum> enumeration;
    utils::Shared<Namespace> namespace_;
    utils::Shared<Module> module;

    Type type = Type::null();

    Value(llvm::Value* value, bool is_constant = false, llvm::Value* self = nullptr);
    
    bool is_null();
    llvm::Value* unwrap(Location location);

    static Value null();

    static Value from_function(utils::Shared<Function> function, llvm::Value* self = nullptr);
    static Value from_struct(utils::Shared<Struct> structure);
    static Value from_module(utils::Shared<Module> module);
    static Value from_namespace(utils::Shared<Namespace> namespace_);
    static Value from_enum(utils::Shared<Enum> enumeration);
    static Value from_type(Type type);

    static Value as_early_function_call();
    static Value as_reference(
        llvm::Value* value, bool is_immutable = false, bool is_stack_allocated = true
    );
};

#endif