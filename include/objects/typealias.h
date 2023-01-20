#ifndef _OBJECTS_TYPEALIAS_H
#define _OBJECTS_TYPEALIAS_H

#include "utils/pointer.h"
#include "objects/enums.h"
#include "lexer/tokens.h"

#include "llvm/IR/Type.h"

struct TypeAlias {
    std::string name;
    llvm::Type* type;

    utils::Ref<Enum> enumeration;

    Span span;

    static TypeAlias from_enum(utils::Ref<Enum> enumeration);
    static TypeAlias null();

    bool is_null();
};

#endif