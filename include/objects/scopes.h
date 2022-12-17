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

    static ScopeLocal from_variable(const Variable& variable, bool use_store_value = false);
    static ScopeLocal from_constant(const Constant& constant, bool use_store_value = false);
};

struct Scope {
    std::string name;
    ScopeType type;

    Scope* parent;
    std::vector<Scope*> children;

    std::map<std::string, Variable> variables;
    std::map<std::string, Constant> constants;
    std::map<std::string, utils::Shared<Function>> functions;
    std::map<std::string, utils::Shared<Struct>> structs;
    std::map<std::string, utils::Shared<Enum>> enums;
    std::map<std::string, utils::Shared<Namespace>> namespaces;
    std::map<std::string, utils::Shared<Module>> modules;
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

    utils::Shared<Function> get_function(std::string name);
    utils::Shared<Struct> get_struct(std::string name);
    utils::Shared<Enum> get_enum(std::string name);
    utils::Shared<Namespace> get_namespace(std::string name);
    utils::Shared<Module> get_module(std::string name);

    TypeAlias get_type(std::string name);

    void exit(Visitor* visitor);

    void finalize();
};


#endif