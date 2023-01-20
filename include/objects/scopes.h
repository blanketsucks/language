#ifndef _OBJECTS_SCOPES_H
#define _OBJECTS_SCOPES_H

#include "utils/pointer.h"

#include "objects/enums.h"
#include "objects/variables.h"
#include "objects/functions.h"
#include "objects/structs.h"
#include "objects/modules.h"
#include "objects/typealias.h"
#include "objects/namespaces.h"

#include <string>
#include <vector>
#include <map>

enum class ScopeType {
    Global,
    Function,
    Anonymous,
    Struct,
    Enum,
    Namespace,
    Module
};

struct ScopeLocal {
    std::string name;

    llvm::Value* value;
    llvm::Type* type;

    bool is_constant;
    bool is_immutable;
    bool is_stack_allocated;
    bool is_scope_local; // If the variable belongs to the current scope and not to a parent of that scope

    bool is_null();
    static ScopeLocal null();

    static ScopeLocal from_variable(const Variable& variable, bool use_store_value = false);
    static ScopeLocal from_constant(const Constant& constant, bool use_store_value = false);
    static ScopeLocal from_scope_local(const ScopeLocal& local, llvm::Value* value, llvm::Type* type = nullptr);

    llvm::Constant* get_constant_value();
};

struct Scope {
    std::string name;
    ScopeType type;

    Scope* parent;
    std::vector<Scope*> children;

    std::map<std::string, Variable> variables;
    std::map<std::string, Constant> constants;
    std::map<std::string, utils::Ref<Function>> functions;
    std::map<std::string, utils::Ref<Struct>> structs;
    std::map<std::string, utils::Ref<Enum>> enums;
    std::map<std::string, utils::Ref<Namespace>> namespaces;
    std::map<std::string, utils::Ref<Module>> modules;
    std::map<std::string, TypeAlias> types;

    Scope(std::string name, ScopeType type, Scope* parent = nullptr);

    ScopeLocal get_local(std::string name, bool use_store_value = true);

    bool has_variable(std::string name);
    bool has_constant(std::string name);
    bool has_function(std::string name);
    bool has_struct(std::string name);
    bool has_enum(std::string name);
    bool has_namespace(std::string name);
    bool has_module(std::string name);
    bool has_type(std::string name);

    Variable get_variable(std::string name);
    Constant get_constant(std::string name);

    utils::Ref<Function> get_function(std::string name);
    utils::Ref<Struct> get_struct(std::string name);
    utils::Ref<Enum> get_enum(std::string name);
    utils::Ref<Namespace> get_namespace(std::string name);
    utils::Ref<Module> get_module(std::string name);

    TypeAlias get_type(std::string name);

    void exit(Visitor* visitor);

    void finalize(bool eliminate_dead_functions = true);
};


#endif