#pragma once

#include <quart/language/symbol.h>
#include <quart/parser/ast.h>

namespace quart {

struct GenericTrait {
    RefPtr<Scope> scope;
    Vector<Type*> arguments;
};

class Trait : public Symbol {
public:
    struct GenericTraitScope {
        RefPtr<Scope> scope;
        Type* type;
    };

    static bool classof(const Symbol* symbol) { return symbol->type() == Symbol::Trait; }

    static RefPtr<Trait> create(String name, TraitType* type, RefPtr<Scope> scope) {
        return RefPtr<Trait>(new Trait(move(name), type, move(scope)));
    }

    TraitType* underlying_type() const { return m_underlying_type; }

    RefPtr<Scope> scope() const { return m_scope; }
    HashMap<String, Span> const& generic_parameters() const { return m_generic_parameters; }

    RefPtr<Scope> resolve_scope(TraitType* type) {
        auto iterator = m_scopes.find(type);
        if (iterator != m_scopes.end()) {
            return iterator->second.scope;
        }

        return m_scope;
    }

    Vector<ast::FunctionExpr const*> const& predefined_functions() const { return m_predefined_functions; }
    void add_predefined_function(ast::FunctionExpr const* function) { m_predefined_functions.push_back(function); }

    class Function const* get_method(const String& name) const;

    void add_generic_parameter(String name, Span span) { m_generic_parameters.insert({ move(name), span }); }
    bool has_generic_parameters() const { return !m_generic_parameters.empty(); }

    HashMap<Type*, GenericTrait>& scopes() { return m_scopes; }
    ErrorOr<GenericTraitScope> create_scope(State&, const Vector<Type*>& types);

    void add_body_expr(ast::Expr* expr) { m_body.push_back(expr); }

private:
    Trait(
        String name, TraitType* type, RefPtr<Scope> scope
    ) : Symbol(move(name), Symbol::Trait, false), m_underlying_type(type), m_scope(move(scope)) {}

    TraitType* m_underlying_type;

    RefPtr<Scope> m_scope;
    HashMap<String, Span> m_generic_parameters;

    Vector<ast::FunctionExpr const*> m_predefined_functions;

    HashMap<Type*, GenericTrait> m_scopes;
    Vector<ast::Expr*> m_body;
};

}