#pragma once

#include <quart/parser/analysis/pass.h>

namespace quart {

class NameResolutionPass : public StaticSemanticsPass {
public:
    enum Mode {
        Declare,
        Resolve
    };

    StringView name() const override { return "NameResolution"; }

    ErrorOr<void> run(Program const& program, State&, bool scope_aware = true) override;

private:
    Mode m_mode = Declare;
    State* m_state = nullptr;

    // NOLINTNEXTLINE
    #define Op(x) ErrorOr<void> visit(ast::x##Expr const&) override;
        ENUMERATE_EXPR_KINDS(Op)
    #undef Op
};

}