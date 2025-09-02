#pragma once

#include <quart/language/symbol.h>
#include <quart/parser/ast.h>

namespace quart {

struct TraitFunction {
    String name;

    Vector<FunctionParameter> parameters;
    Type* return_type;
};

class Trait : public Symbol {
public:
    static bool classof(const Symbol* symbol) { return symbol->type() == Symbol::Trait; }

    static RefPtr<Trait> create(String name) {
        return RefPtr<Trait>(new Trait(move(name)));
    }

    Vector<TraitFunction> const& functions() const { return m_functions; }
    Vector<ast::FunctionExpr const*> const& predefined_functions() const { return m_predefined_functions; }

    void add_function(TraitFunction function) { m_functions.push_back(move(function)); }
    void add_predefined_function(ast::FunctionExpr const* function) { m_predefined_functions.push_back(function); }

private:
    Trait(String name) : Symbol(move(name), Symbol::Trait, false) {}

    Vector<ast::FunctionExpr const*> m_predefined_functions;
    Vector<TraitFunction> m_functions;
};

}