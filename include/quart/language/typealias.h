#pragma once

#include <quart/language/types.h>
#include <quart/lexer/location.h>
#include <quart/parser/ast.h>

namespace quart {

struct TypeAlias {
    std::string name;
    quart::Type* type;

    std::vector<ast::GenericParameter> parameters;
    std::unique_ptr<ast::TypeExpr> expr;

    Span span;

    TypeAlias() = default;
    TypeAlias(
        const std::string& name,
        quart::Type* type,
        const Span& span
    ) : name(name), type(type), span(span), expr(nullptr) {}
    TypeAlias(
        const std::string& name,
        std::vector<ast::GenericParameter> parameters,
        std::unique_ptr<ast::TypeExpr> expr,
        const Span& span
    ) : name(name), type(nullptr), parameters(std::move(parameters)), expr(std::move(expr)), span(span) {}

    bool is_generic() const { return this->type == nullptr; }

};

}