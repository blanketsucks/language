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

    template<typename T>
    Optional<T> evaluate_binary_operation(BinaryOp op, T lhs, T rhs) const;

    Constant* evaluate_binary_operation(BinaryOp op, Constant* lhs, Constant* rhs) const;

    State& m_state; // NOLINT

    bool m_in_loop = false;
    bool m_should_break = false;
    bool m_should_continue = false;
};

}