#pragma once

#include <quart/common.h>

#include <llvm/Support/FormatVariadic.h>

namespace quart {

class Scope;
class Module;

class Symbol {
public:
    NO_COPY(Symbol)
    DEFAULT_MOVE(Symbol)

    enum SymbolType : u8 {
        Variable,
        Function,
        Struct,
        Enum,
        TypeAlias,
        Module,
        Trait
    };

    static String parse_qualified_name(Symbol*, RefPtr<Scope>);

    Symbol(String name, SymbolType type, bool is_public) : m_name(move(name)), m_type(type), m_is_public(is_public) {}
    virtual ~Symbol() = default;

    String const& name() const { return m_name; }
    SymbolType type() const { return m_type; }

    StringView str() const;

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

    bool is_public() const { return m_is_public; }

    class Module* module() const { return m_module; }
    void set_module(class Module* module) { m_module = module; }

private:
    String m_name;
    SymbolType m_type;

    bool m_is_public = false;
    class Module* m_module = nullptr;
};

}