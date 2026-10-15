#include <quart/parser/analysis/name_resolution.h>
#include <quart/state.h>

namespace quart {

ErrorOr<void> NameResolutionPass::run(Program const& program, State& state) {
    m_state = &state;

    m_mode = Declare;
    TRY(StaticSemanticsPass::run(program, state));

    m_mode = Resolve;
    TRY(StaticSemanticsPass::run(program, state));

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::BlockExpr const& expr) {
    for (auto& e : expr.block()) {
        TRY(walk(*e));
    }

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::ExternBlockExpr const& expr) {
    for (auto& e : expr.block()) {
        TRY(walk(*e));
    }

    return {};
}

EMPTY_VISIT_IMPL(NameResolutionPass, Integer);
EMPTY_VISIT_IMPL(NameResolutionPass, Float);
EMPTY_VISIT_IMPL(NameResolutionPass, String);

ErrorOr<void> NameResolutionPass::visit(ast::IdentifierExpr const& expr) {
    auto scope = m_state->scope();
    auto* symbol = scope->resolve(expr.name());

    if (!symbol) {
        m_state->add_error_diagnostic(err(
            expr.span(), "Unknown identifier '{}'", expr.name()
        ));

        return {};
    } else if (!isa<Variable>(symbol) && !isa<Function>(symbol)) {
        m_state->add_error_diagnostic(err(
            expr.span(), "'{}' does not refer to a value", expr.name()
        ));
    
        return {};
    }

    expr.semantic_info().scope = scope.get();
    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::AssignmentExpr const& expr) {
    auto scope = m_state->scope();

    auto& ident = expr.identifier();
    if (scope->resolve(ident.value)) {
        m_state->add_error_diagnostic(err(
            expr.span(), "'{}' is already defined", ident.value
        ));

        return {};
    }

    if (expr.value()) {
        TRY(walk(*expr.value()));
    }

    u8 flags = Variable::None;
    if (ident.is_mutable) {
        flags |= Variable::Mutable;
    }

    if (expr.is_public()) {
        flags |= Variable::Public;
    }

    scope->add_symbol(Variable::create(ident.value, flags));
    expr.semantic_info().scope = scope.get();

    return {};
}

// TODO: Implement
EMPTY_VISIT_IMPL(NameResolutionPass, TupleAssignment);
EMPTY_VISIT_IMPL(NameResolutionPass, Const);

ErrorOr<void> NameResolutionPass::visit(ast::ArrayExpr const& expr) {
    for (auto& e : expr.elements()) {
        TRY(walk(*e));
    }

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::UnaryOpExpr const& expr) {
    return walk(expr.value());
}

ErrorOr<void> NameResolutionPass::visit(ast::ReferenceExpr const& expr) {
    return walk(expr.value());
}

ErrorOr<void> NameResolutionPass::visit(ast::BinaryOpExpr const& expr) {
    TRY(walk(expr.lhs()));
    TRY(walk(expr.rhs()));

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::InplaceBinaryOpExpr const& expr) {
    TRY(walk(expr.lhs()));
    TRY(walk(expr.rhs()));

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::CallExpr const& expr) {
    TRY(walk(expr.callee()));
    for (auto& argument : expr.args()) {
        TRY(walk(*argument));
    }

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::ReturnExpr const& expr) {
    if (!expr.value()) {
        return {};
    }

    return walk(*expr.value());
}

ErrorOr<void> NameResolutionPass::visit(ast::FunctionDeclExpr const& expr) {
    if (m_mode == Resolve) {
        return {};
    }

    auto current_scope = m_state->scope();
    if (current_scope->resolve(expr.name())) {
        m_state->add_error_diagnostic(err(
            expr.span(), "'{}' is already defined", expr.name()
        ));

        return {};
    }

    Vector<FunctionParameter> parameters;
    u32 index = 0;

    for (auto& p : expr.parameters()) {
        parameters.emplace_back(p.name, nullptr, p.flags, index, p.span);
        index++;
    }

    auto scope = Scope::create({}, ScopeType::Function, current_scope);
    auto function = Function::create(
        expr.span(),
        expr.name(),
        parameters,
        nullptr,
        scope,
        expr.linkage(),
        nullptr,
        expr.is_public()
    );

    current_scope->add_symbol(move(function));
    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::FunctionExpr const& expr) {
    if (m_mode == Declare) {
        TRY(visit(expr.decl()));
        return {};
    }

    auto current_scope = m_state->scope();
    auto* function = current_scope->resolve<Function>(expr.decl().name());

    m_state->set_current_scope(function->scope());
    for (auto& p : function->parameters()) {
        function->scope()->add_symbol(Variable::create(p.name));
    }

    TRY(visit(expr.body()));
    m_state->set_current_scope(current_scope);

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::DeferExpr const& expr) {
    return walk(expr.expr());
}

ErrorOr<void> NameResolutionPass::visit(ast::IfExpr const& expr) {
    TRY(walk(expr.condition()));
    TRY(walk(expr.body()));

    if (expr.else_body()) {
        TRY(walk(*expr.else_body()));
    }

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::WhileExpr const& expr) {
    TRY(walk(expr.condition()));
    TRY(walk(expr.body()));

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::ForExpr const& expr) {
    TRY(walk(expr.iterable()));
    TRY(walk(expr.body()));

    return {};
}

EMPTY_VISIT_IMPL(NameResolutionPass, Break);
EMPTY_VISIT_IMPL(NameResolutionPass, Continue);

EMPTY_VISIT_IMPL(NameResolutionPass, Struct);
EMPTY_VISIT_IMPL(NameResolutionPass, Constructor);

ErrorOr<void> NameResolutionPass::visit(ast::AttributeExpr const& expr) {
    return walk(expr.parent());
}

ErrorOr<void> NameResolutionPass::visit(ast::IndexExpr const& expr) {
    TRY(walk(expr.value()));
    TRY(walk(expr.index()));

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::CastExpr const& expr) {
    return walk(expr.value());
}

ErrorOr<void> NameResolutionPass::visit(ast::SizeofExpr const& expr) {
    return walk(expr.value());
}

ErrorOr<void> NameResolutionPass::visit(ast::OffsetofExpr const& expr) {
    return walk(expr.value());
}

EMPTY_VISIT_IMPL(NameResolutionPass, Path);

EMPTY_VISIT_IMPL(NameResolutionPass, Using);

ErrorOr<void> NameResolutionPass::visit(ast::TupleExpr const& expr) {
    for (auto& e : expr.elements()) {
        TRY(walk(*e));
    }

    return {};
}

EMPTY_VISIT_IMPL(NameResolutionPass, Enum);
EMPTY_VISIT_IMPL(NameResolutionPass, Import);

ErrorOr<void> NameResolutionPass::visit(ast::TernaryExpr const& expr) {
    TRY(walk(expr.condition()));
    TRY(walk(expr.true_expr()));
    TRY(walk(expr.false_expr()));

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::ArrayFillExpr const& expr) {
    TRY(walk(expr.value()));
    TRY(walk(expr.count()));

    return {};
}

EMPTY_VISIT_IMPL(NameResolutionPass, TypeAlias);

ErrorOr<void> NameResolutionPass::visit(ast::StaticAssertExpr const& expr) {
    return walk(expr.condition());
}

ErrorOr<void> NameResolutionPass::visit(ast::MaybeExpr const& expr) {
    return walk(expr.value());
}

EMPTY_VISIT_IMPL(NameResolutionPass, Module);
EMPTY_VISIT_IMPL(NameResolutionPass, Impl);
EMPTY_VISIT_IMPL(NameResolutionPass, Trait);
EMPTY_VISIT_IMPL(NameResolutionPass, ImplTrait);

ErrorOr<void> NameResolutionPass::visit(ast::MatchExpr const& expr) {
    TRY(walk(expr.value()));
    for (auto& arm : expr.arms()) {
        TRY(walk(*arm.body));
    }

    return {};
}

ErrorOr<void> NameResolutionPass::visit(ast::RangeForExpr const& expr) {
    TRY(walk(expr.start()));
    TRY(walk(expr.end()));
    TRY(walk(expr.body()));

    return {};
}

EMPTY_VISIT_IMPL(NameResolutionPass, Bool);

ErrorOr<void> NameResolutionPass::visit(ast::ConstEvalExpr const& expr) {
    for (auto& e : expr.body()) {
        TRY(walk(*e));
    }

    return {};
}

}