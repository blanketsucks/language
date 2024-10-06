#pragma once

#include <quart/language/types.h>
#include <quart/language/symbol.h>
#include <quart/parser/ast.h>

namespace quart {

using GenericCache = HashMap<Vector<Type*>, Type*>;

struct GenericTypeParameter {
    String name;

    Vector<Type*> constraints;
    Type* default_type;

    Span span;

    bool is_optional() const { return default_type != nullptr; }  
};

class TypeAlias : public Symbol {
public:
    static RefPtr<TypeAlias> create(String name, Type* type) {
        return RefPtr<TypeAlias>(new TypeAlias(move(name), type));
    }

    static RefPtr<TypeAlias> create(String name, Vector<GenericTypeParameter> parameters, ast::TypeExpr* expr) {
        return RefPtr<TypeAlias>(new TypeAlias(move(name), move(parameters), expr));
    }

    Type* underlying_type() const { return m_underlying_type; }

    Vector<GenericTypeParameter> const& parameters() const { return m_parameters; }
    ast::TypeExpr const& expr() const { return *m_expr; }

    GenericCache const& cache() const { return m_cache; }

    bool is_generic() const { return m_underlying_type == nullptr; }
    bool all_parameters_have_default() const; // FIXME: Find a better name for this

    ErrorOr<Type*> evaluate(State&);
    ErrorOr<Type*> evaluate(State&, const Vector<Type*>& args);

private:
    TypeAlias(String name, Type* type) : Symbol(move(name), Symbol::TypeAlias), m_underlying_type(type) {}
    TypeAlias(
        String name, Vector<GenericTypeParameter> parameters, ast::TypeExpr* expr
    ) : Symbol(move(name), Symbol::TypeAlias), m_parameters(move(parameters)), m_expr(expr) {}

    Type* m_underlying_type = nullptr;

    Vector<GenericTypeParameter> m_parameters;
    ast::TypeExpr* m_expr = nullptr;

    GenericCache m_cache;
};

}