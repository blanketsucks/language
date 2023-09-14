#pragma once

#include <quart/language/types.h>
#include <quart/lexer/location.h>

namespace quart {

struct TypeAlias {
    std::string name;
    quart::Type* type;

    Span span;

    TypeAlias() = default;
    TypeAlias(const std::string& name, quart::Type* type, const Span& span) : name(name), type(type), span(span) {}
};

}