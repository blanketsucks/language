#pragma once

#include <quart/utils/pointer.h>
#include <quart/lexer/location.h>
#include <quart/objects/enums.h>

#include <llvm/IR/Type.h>

struct TypeAlias {
    std::string name;
    llvm::Type* type;

    utils::Ref<Enum> enumeration;

    Span span;

    static TypeAlias from_enum(utils::Ref<Enum> enumeration);
    static TypeAlias null();

    bool is_null();
};
