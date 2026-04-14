#pragma once

#include <quart/common.h>
#include <quart/source_code.h>
#include <quart/language/types.h>

namespace quart {

struct GenericTypeParameter {
    String name;

    Vector<Type*> constraints;
    Type* default_type = nullptr;

    Span span;

    GenericTypeParameter(String name, Span span) : name(move(name)), span(span) {}
    GenericTypeParameter(String name, Vector<Type*> constraints, Type* default_type, Span span)
        : name(move(name)), constraints(move(constraints)), default_type(default_type), span(span) {}

    bool is_optional() const { return default_type != nullptr; }  
};


}