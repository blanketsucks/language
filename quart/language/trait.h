#pragma once

#include <quart/language/symbol.h>
#include <quart/parser/ast.h>

namespace quart {

class Trait : public Symbol {
public:
    static bool classof(const Symbol* symbol) { return symbol->type() == Symbol::Trait; }

    static RefPtr<Trait> create(String name, TraitType* type, RefPtr<Scope> scope) {
        return RefPtr<Trait>(new Trait(move(name), type, move(scope)));
    }

    TraitType* underlying_type() const { return m_underlying_type; }

    RefPtr<Scope> scope() const { return m_scope; }

    Vector<ast::FunctionExpr const*> const& predefined_functions() const { return m_predefined_functions; }
    void add_predefined_function(ast::FunctionExpr const* function) { m_predefined_functions.push_back(function); }

    class Function const* get_method(const String& name) const;

private:
    Trait(
        String name, TraitType* type, RefPtr<Scope> scope
    ) : Symbol(move(name), Symbol::Trait, false), m_underlying_type(type), m_scope(move(scope)) {}

    TraitType* m_underlying_type;

    RefPtr<Scope> m_scope;

    Vector<ast::FunctionExpr const*> m_predefined_functions;
};

}