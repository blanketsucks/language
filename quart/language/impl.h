#pragma once

#include <quart/common.h>
#include <quart/language/scopes.h>

namespace quart {

class Impl {
public:
    static OwnPtr<Impl> create(Type* underlying_type, RefPtr<Scope> scope) {
        return OwnPtr<Impl>(new Impl(underlying_type, move(scope)));
    }

    static OwnPtr<Impl> create(Type* underlying_type, RefPtr<Scope> scope, ast::BlockExpr* body, Set<String> parameters) {
        return OwnPtr<Impl>(new Impl(underlying_type, move(scope), body, move(parameters)));
    }

    Type* underlying_type() const { return m_underlying_type; }
    RefPtr<Scope> scope() const { return m_scope; }

    ast::BlockExpr* body() const { return m_body; }

    bool is_generic() const { return !m_generic_parameters.empty(); }

    ErrorOr<RefPtr<Scope>> make(State&, Type*);

private:
    Impl(
        Type* underlying_type, RefPtr<Scope> scope
    ) : m_underlying_type(underlying_type), m_scope(move(scope)) {}

    Impl(
        Type* underlying_type, RefPtr<Scope> scope, ast::BlockExpr* body, Set<String> parameters
    ) : m_underlying_type(underlying_type), m_scope(move(scope)), m_body(body), m_generic_parameters(move(parameters)) {}

    Type* m_underlying_type = nullptr;
    RefPtr<Scope> m_scope = nullptr;

    HashMap<Type*, RefPtr<Scope>> m_impls;
    ast::BlockExpr* m_body = nullptr;
    Set<String> m_generic_parameters;
};

}