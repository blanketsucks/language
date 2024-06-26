#pragma once

#include <quart/language/variables.h>
#include <quart/language/functions.h>
#include <quart/language/structs.h>
#include <quart/language/enums.h>
#include <quart/language/typealias.h>
#include <quart/language/modules.h>
#include <quart/language/types.h>
#include <quart/language/variables.h>
#include <quart/language/symbol.h>
#include <quart/parser/ast.h>

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

// struct ScopeLocal {
//     enum Flags : u8 {
//         None,
//         Constant       = 1 << 0,
//         Mutable        = 1 << 1,
//         StackAllocated = 1 << 2,
//         LocalToScope   = 1 << 3
//     };

//     std::string name;

//     llvm::Value* value;
//     quart::Type* type;

//     u8 flags;

//     bool is_null();
//     static ScopeLocal null();

//     static ScopeLocal from_variable(const quart::Variable& variable, bool use_store_value = false);
//     static ScopeLocal from_constant(const quart::Constant& constant, bool use_store_value = false);
//     static ScopeLocal from_scope_local(const ScopeLocal& local, llvm::Value* value, quart::Type* type = nullptr);

//     inline bool is_mutable() const { return this->flags & Flags::Mutable; }
//     inline bool is_constant() const { return this->flags & Flags::Constant; }

//     llvm::Constant* get_constant_value();
// };

class Scope {
public:
    static Scope* create(String name, ScopeType type, Scope* parent = nullptr);

    String const& name() const { return m_name; }
    ScopeType type() const { return m_type; }

    Scope* parent() const { return m_parent; }
    Vector<Scope*> const& children() const { return m_children; }

    void* data() const { return m_data; }

    Symbol* resolve(const String& name);
    template<typename T> T* resolve(const String& name) {
        auto* symbol = this->resolve(name);
        if (!symbol) {
            return nullptr;
        }

        return symbol->as<T>();
    }

    template<typename T> T* as() const;
    template<typename T> bool is() const;

    // ScopeLocal get_local(const std::string& name, bool use_store_value = true);

    void finalize(bool eliminate_dead_functions = true);

private:
    Scope(String name, ScopeType type, Scope* parent) : m_name(move(name)), m_type(type), m_parent(parent) {}

    String m_name;
    ScopeType m_type;

    Scope* m_parent;
    Vector<Scope*> m_children;

    HashMap<String, RefPtr<Symbol>> m_symbols;
    
    Vector<ast::Expr*> m_defers;

    void* m_data = nullptr;
};

}