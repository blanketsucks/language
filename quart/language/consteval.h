#pragma once

#include <quart/common.h>
#include <quart/language/types.h>
#include <quart/language/constants.h>

#include <quart/parser/ast.h>

namespace quart {

class State;

class ConstantEvaluator {
public:
    ConstantEvaluator(State& state) : m_state(state) {}

    bool is_constant_expression(ast::Expr const& expr) const;
    ErrorOr<Constant*> evaluate(ast::Expr const& expr);

private:

    // NOLINTNEXTLINE
#define Op(x) bool is_constant_expression(ast::x##Expr const&) const;
    ENUMERATE_EXPR_KINDS(Op)
#undef Op

    // NOLINTNEXTLINE
#define Op(x) ErrorOr<Constant*> evaluate(ast::x##Expr const&);
    ENUMERATE_EXPR_KINDS(Op)
#undef Op

    State& m_state; // NOLINT
};

}