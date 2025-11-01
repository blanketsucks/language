#pragma once

#include <quart/parser/ast.h>

namespace quart {

class State;

class TypeChecker {
public:
    explicit TypeChecker(State& state) : m_state(state) {}

    ErrorOr<Type*> type_check(ast::Expr const& expr);

    // NOLINTNEXTLINE
    #define Op(x) ErrorOr<Type*> type_check(ast::x##Expr const&);
        ENUMERATE_EXPR_KINDS(Op)
    #undef Op

private:

    ErrorOr<Type*> resolve_reference(Scope& scope, Span span, const String& name, bool is_mutable);
    ErrorOr<Type*> resolve_reference(ast::Expr const& expr, bool is_mutable = false);

    ErrorOr<Type*> type_check_attribute_access(ast::AttributeExpr const& expr, bool as_reference, bool as_mutable);
    ErrorOr<Type*> type_check_index_access(ast::IndexExpr const& expr, bool as_reference, bool as_mutable);

    State& m_state; // NOLINT

    bool m_has_self = false;
};

}