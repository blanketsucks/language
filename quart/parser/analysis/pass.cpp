#include <quart/parser/analysis/pass.h>
#include <quart/state.h>

namespace quart {

ErrorOr<void> StaticSemanticsPass::run(Program const& program, State& state, bool scope_aware) {
    if (!scope_aware) {
        for (auto& module : program.modules()) {
            for (auto& stmt : module->stmts()) {
                TRY(this->walk(*stmt));
            }
        }

        return {};
    }

    for (auto& module : program.modules()) {
        auto& path = module->path();
    
        auto mod = state.loader().lookup(path);
        state.push_scope(mod->scope());

        for (auto& stmt : module->stmts()) {
            TRY(walk(*stmt));
        }

        state.pop_scope();
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