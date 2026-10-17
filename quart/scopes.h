#pragma once

#include <quart/symbols/variables.h>
#include <quart/symbols/functions.h>
#include <quart/symbols/structs.h>
#include <quart/symbols/enums.h>
#include <quart/symbols/type_alias.h>
#include <quart/symbols/modules.h>
#include <quart/symbols/variables.h>
#include <quart/parser/ast.h>
#include <quart/casting.h>
#include <quart/symbol.h>
#include <quart/types.h>

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
    ~Scope() = default;

    NO_MOVE(Scope);
    NO_COPY(Scope);

    static RefPtr<Scope> create(String name, ScopeType type, Scope* parent = nullptr);

    RefPtr<Scope> clone(String name);

    String const& name() const { return m_name; }
    ScopeType type() const { return m_type; }

    Scope* parent() const { return m_parent; }
    Vector<RefPtr<Scope>> const& children() const { return m_children; }

    void add_child(RefPtr<Scope> scope) { m_children.push_back(move(scope)); }

    Module* module() const { return m_module; }
    void set_module(Module* module) { m_module = module; }

    HashMap<String, RefPtr<Symbol>> const& symbols() const { return m_symbols; }

    Symbol* resolve(const String& name);
    template<typename T> T* resolve(const String& name) {
        auto* symbol = this->resolve(name);
        if (!symbol) {
            return nullptr;
        }

        return cast<T>(symbol);
    }

    void add_symbol(RefPtr<Symbol> symbol) {
        m_symbols[symbol->name()] = move(symbol);
    }

    void remove_symbol(const String& name) { m_symbols.erase(name); }

    void finalize(bool eliminate_dead_functions = true);

private:
    Scope(String name, ScopeType type, Scope* parent) : m_name(move(name)), m_type(type), m_parent(parent) {}

    String m_name;
    ScopeType m_type;

    Scope* m_parent;
    Vector<RefPtr<Scope>> m_children;

    HashMap<String, RefPtr<Symbol>> m_symbols;
    
    Module* m_module = nullptr;
};

}