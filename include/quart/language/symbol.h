#pragma once

#include <quart/common.h>

namespace quart {

class Scope;

class Symbol {
public:
    NO_COPY(Symbol)
    DEFAULT_MOVE(Symbol)

    static bool classof(Symbol const*) { return true; }

    enum SymbolType {
        Variable,
        Function,
        Struct,
        Enum,
        TypeAlias,
        Module
    };

    Symbol(String name, SymbolType type) : m_name(move(name)), m_type(type) {}
    virtual ~Symbol() = default;

    String const& name() const { return m_name; }
    SymbolType type() const { return m_type; }

    bool is_variable() const { return m_type == SymbolType::Variable; }
    bool is_function() const { return m_type == SymbolType::Function; }
    bool is_struct() const { return m_type == SymbolType::Struct; }
    bool is_enum() const { return m_type == SymbolType::Enum; }
    bool is_type_alias() const { return m_type == SymbolType::TypeAlias; }
    bool is_module() const { return m_type == SymbolType::Module; }

    template<typename T> requires(std::is_base_of_v<Symbol, T>)
    T* as() {
        if (!T::classof(this)) {
            return nullptr;
        }
        
        return static_cast<T*>(this);
    }

    template<typename T> requires(std::is_base_of_v<Symbol, T>)
    bool is() const { return T::classof(this); }

    bool is(SymbolType type) const { return m_type == type; }

    template<typename ...Args> requires(of_type_v<SymbolType, Args...>)
    bool is(Args... args) const {
        return (... || (m_type == args));
    }

private:
    String m_name;
    SymbolType m_type;
};

}