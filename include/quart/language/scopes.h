#pragma once

#include <quart/utils/pointer.h>
#include <quart/language/variables.h>
#include <quart/language/functions.h>
#include <quart/language/structs.h>
#include <quart/language/enums.h>
#include <quart/language/typealias.h>
#include <quart/language/modules.h>

#include <quart/language/types.h>
#include <quart/language/variables.h>

#include <string>
#include <vector>
#include <map>

namespace quart {

enum class ScopeType {
    Global,
    Function,
    Anonymous,
    Struct,
    Enum,
    Namespace,
    Module,
    Impl
};

struct ScopeLocal {
    enum Flags {
        None,
        Constant       = 1 << 0,
        Mutable        = 1 << 1,
        StackAllocated = 1 << 2,
        LocalToScope   = 1 << 3
    };

    std::string name;

    llvm::Value* value;
    quart::Type* type;

    uint8_t flags;

    bool is_null();
    static ScopeLocal null();

    static ScopeLocal from_variable(const quart::Variable& variable, bool use_store_value = false);
    static ScopeLocal from_constant(const quart::Constant& constant, bool use_store_value = false);
    static ScopeLocal from_scope_local(const ScopeLocal& local, llvm::Value* value, quart::Type* type = nullptr);

    inline bool is_mutable() const { return this->flags & Flags::Mutable; }
    inline bool is_constant() const { return this->flags & Flags::Constant; }

    llvm::Constant* get_constant_value();
};

struct Scope {
    template<typename T> using ValueMap = std::map<std::string, T>;

    std::string name;
    ScopeType type;

    Scope* parent;
    std::vector<Scope*> children;

    ValueMap<Variable> variables;
    ValueMap<Constant> constants;
    ValueMap<FunctionRef> functions;
    ValueMap<StructRef> structs;
    ValueMap<EnumRef> enums;
    ValueMap<ModuleRef> modules;
    ValueMap<TypeAlias> type_aliases;

    Scope(const std::string& name, ScopeType type, Scope* parent = nullptr);

    ScopeLocal get_local(const std::string& name, bool use_store_value = true);

    bool has_variable(const std::string& name);
    bool has_constant(const std::string& name);
    bool has_function(const std::string& name);
    bool has_struct(const std::string& name);
    bool has_enum(const std::string& name);
    bool has_module(const std::string& name);
    bool has_type_alias(const std::string& name);

    Variable* get_variable(const std::string& name);
    Constant* get_constant(const std::string& name);

    FunctionRef get_function(const std::string& name);
    StructRef get_struct(const std::string& name);
    EnumRef get_enum(const std::string& name);
    ModuleRef get_module(const std::string& name);

    TypeAlias* get_type_alias(const std::string& name);

    void exit(Visitor* visitor);

    void finalize(bool eliminate_dead_functions = true);
};

}