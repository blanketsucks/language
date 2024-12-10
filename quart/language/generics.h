#pragma once

#include <quart/common.h>
#include <quart/source_code.h>
#include <quart/language/types.h>

namespace quart {

struct GenericTypeParameter {
    String name;

    Vector<Type*> constraints;
    Type* default_type;

    Span span;

    bool is_optional() const { return default_type != nullptr; }  
};


}