#pragma once

#include <quart/common.h>
#include <quart/errors.h>
#include <quart/parser/imports.h>
#include <quart/parser/ast.h>


#include <functional>

namespace quart {

// NOLINTNEXTLINE
#define EMPTY_VISIT_IMPL(cls, x) ErrorOr<void> cls::visit(ast::x##Expr const&) { return {}; } 

class StaticSemanticsPass {
public:
    virtual ~StaticSemanticsPass() = default;
    StaticSemanticsPass() = default;

    NO_COPY(StaticSemanticsPass);
    NO_MOVE(StaticSemanticsPass);

    virtual StringView name() const = 0;

    virtual ErrorOr<void> run(Program const& program, State&, bool scope_aware = false);

protected:
    ErrorOr<void> walk(ast::Expr const& expr);

    // NOLINTNEXTLINE
    #define Op(x) virtual ErrorOr<void> visit(ast::x##Expr const&) = 0;
        ENUMERATE_EXPR_KINDS(Op)
    #undef Op
};

class StaticSemanticsPassManager {
public:
    template<typename T, typename ...Args> requires(std::is_base_of_v<StaticSemanticsPass, T>)
    void add(Args&&... args) {
        m_passes.push_back(make<T>(std::forward<Args>(args)...));
    }

    std::function<ErrorOr<void>()>& after_pass_callback() {
        return m_after_pass_callback;
    }

    void set_after_pass_callback(std::function<ErrorOr<void>()> callback) {
        m_after_pass_callback = move(callback);
    }

    ErrorOr<void> run(Program const& program, State& state) const {
        for (auto& pass : m_passes) {
            TRY(pass->run(program, state));
            if (m_after_pass_callback) {
                TRY(m_after_pass_callback());
            }
        }

        return {};
    }

private:
    Vector<OwnPtr<StaticSemanticsPass>> m_passes;
    std::function<ErrorOr<void>()> m_after_pass_callback;
};

}