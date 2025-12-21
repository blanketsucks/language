#pragma once

#include <quart/language/variables.h>
#include <quart/language/functions.h>
#include <quart/language/structs.h>
#include <quart/language/enums.h>
#include <quart/language/type_alias.h>
#include <quart/language/modules.h>
#include <quart/language/types.h>
#include <quart/language/variables.h>
#include <quart/language/symbol.h>
#include <quart/parser/ast.h>

#include <string>
#include <vector>
#include <map>

namespace quart {

enum class ScopeType : u8 {
    Global,
    Function,
    Anonymous,
    Struct,
    Enum,
    Namespace,
    Module,
    Impl
};

class Scope {
public:
    static RefPtr<Scope> create(String name, ScopeType type, RefPtr<Scope> parent = nullptr);

    RefPtr<Scope> clone(String name);

    String const& name() const { return m_name; }
    ScopeType type() const { return m_type; }

    RefPtr<Scope> parent() const { return m_parent; }
    Vector<RefPtr<Scope>> const& children() const { return m_children; }

    Module* module() const { return m_module; }
    void set_module(Module* module) { m_module = module; }

    HashMap<String, RefPtr<Symbol>> const& symbols() const { return m_symbols; }

    Symbol* resolve(const String& name);
    template<typename T> T* resolve(const String& name) {
        auto* symbol = this->resolve(name);
        if (!symbol) {
            return nullptr;
        }

        return symbol->as<T>();
    }

    void add_symbol(RefPtr<Symbol> symbol) {
        m_symbols[symbol->name()] = move(symbol);
    }

    void remove_symbol(const String& name) { m_symbols.erase(name); }

    void finalize(bool eliminate_dead_functions = true);

private:
    Scope(String name, ScopeType type, RefPtr<Scope> parent) : m_name(move(name)), m_type(type), m_parent(move(parent)) {}

    String m_name;
    ScopeType m_type;

    RefPtr<Scope> m_parent;
    Vector<RefPtr<Scope>> m_children;

    HashMap<String, RefPtr<Symbol>> m_symbols;
    
    Module* m_module = nullptr;
};

}