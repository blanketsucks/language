#pragma once

#include <quart/language/types.h>
#include <quart/lexer/location.h>
#include <quart/parser/ast.h>

namespace quart {

struct GenericTypeParameter {
    std::string name;

    std::vector<quart::Type*> constraints;
    quart::Type* default_type;

    Span span;

    bool is_optional() const { return this->default_type != nullptr; }  
};

// Cache for generic types so we don't have to re-instantiate them every time.
// Probably needs to be moved elsewhere.
using GenericCache = std::map<std::vector<quart::Type*>, quart::Type*>;

struct TypeAlias {
    std::string name;
    quart::Type* type;

    std::vector<GenericTypeParameter> parameters;
    std::unique_ptr<ast::TypeExpr> expr;

    GenericCache cache;

    Span span;

    TypeAlias() = default;
    TypeAlias(
        const std::string& name,
        quart::Type* type,
        const Span& span
    ) : name(name), type(type), span(span), expr(nullptr) {}
    TypeAlias(
        const std::string& name,
        std::vector<GenericTypeParameter> parameters,
        std::unique_ptr<ast::TypeExpr> expr,
        const Span& span
    ) : name(name), type(nullptr), parameters(parameters), expr(std::move(expr)), span(span) {}

    bool is_generic() const { return this->type == nullptr; }

};

}