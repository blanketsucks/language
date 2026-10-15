#include <quart/parser/analysis/pass.h>

namespace quart {

ErrorOr<void> StaticSemanticsPass::run(Program const& program, State&) {
    for (auto& module : program.modules()) {
        for (auto& stmt : module->stmts()) {
            TRY(this->walk(*stmt));
        }
    }

    return {};
}

ErrorOr<void> StaticSemanticsPass::walk(ast::Expr const& expr) {
    switch (expr.kind()) {
    // NOLINTNEXTLINE
    #define Op(x) case ast::ExprKind::x: return this->visit(static_cast<ast::x##Expr const&>(expr));
        ENUMERATE_EXPR_KINDS(Op)
    #undef Op
    }

    ASSERT(false, "Unreachable");
}



}