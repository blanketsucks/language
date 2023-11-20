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

    [[nodiscard]] bool is_optional() const { return this->default_type != nullptr; }  
};

// Cache for generic types so we don't have to re-instantiate them every time.
// Probably needs to be moved elsewhere.
using GenericCache = std::map<std::vector<quart::Type*>, quart::Type*>;

struct TypeAlias {
    std::string name;
    quart::Type* type = nullptr;

    std::vector<GenericTypeParameter> parameters;
    OwnPtr<ast::TypeExpr> expr;

    GenericCache cache;

    Span span;

    TypeAlias() = default;
    TypeAlias(
        const std::string& name,
        quart::Type* type,
        const Span& span
    ) : name(name), type(type), span(span) {}
    TypeAlias(
        const std::string& name,
        std::vector<GenericTypeParameter> parameters,
        OwnPtr<ast::TypeExpr> expr,
        const Span& span
    ) : name(name), parameters(std::move(parameters)), expr(std::move(expr)), span(span) {}

    bool is_generic() const { return this->type == nullptr; }

    bool is_instantiable_without_args() const;

    quart::Type* instantiate(Visitor&);
    quart::Type* instantiate(Visitor&, const std::vector<quart::Type*>& args);
};

}