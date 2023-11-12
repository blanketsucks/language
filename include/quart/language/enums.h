#pragma once

#include <quart/language/types.h>
#include <quart/lexer/location.h>

#include <llvm/IR/Value.h>

#include <map>

namespace quart {

struct Scope;

struct Enumerator {
    std::string name;

    llvm::Constant* value;
    quart::Type* type;
};

struct Enum {
    std::string name;
    quart::Type* type;

    Scope* scope;

    std::map<std::string, Enumerator> enumerators;

    Enum(const std::string& name, quart::Type* type);

    void add_enumerator(const std::string& name, llvm::Constant* value, const Span& span);
    bool has_enumerator(const std::string& name);

    Enumerator* get_enumerator(const std::string& name);
};

using EnumRef = RefPtr<Enum>;

}