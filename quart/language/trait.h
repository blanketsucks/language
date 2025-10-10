#pragma once

#include <quart/language/symbol.h>
#include <quart/parser/ast.h>

namespace quart {

struct TraitFunction {
    String name;
    Vector<FunctionParameter> parameters;
    Type* return_type;

    FunctionType* type;
    Span span;
};

class Trait : public Symbol {
public:
    static bool classof(const Symbol* symbol) { return symbol->type() == Symbol::Trait; }

    static RefPtr<Trait> create(String name, TraitType* type) {
        return RefPtr<Trait>(new Trait(move(name), type));
    }

    TraitType* underlying_type() const { return m_underlying_type; }

    HashMap<String, TraitFunction> const& functions() const { return m_functions; }
    Vector<ast::FunctionExpr const*> const& predefined_functions() const { return m_predefined_functions; }

    void add_function(TraitFunction function) { m_functions[function.name] = move(function); }
    void add_predefined_function(ast::FunctionExpr const* function) { m_predefined_functions.push_back(function); }

    TraitFunction const* get_function(const String& name) const {
        auto iterator = m_functions.find(name);
        if (iterator == m_functions.end()) {
            return nullptr;
        }

        return &iterator->second;
    }

private:
    Trait(String name, TraitType* type) : Symbol(move(name), Symbol::Trait, false), m_underlying_type(type) {}

    TraitType* m_underlying_type;

    Vector<ast::FunctionExpr const*> m_predefined_functions;
    HashMap<String, TraitFunction> m_functions;
};

}