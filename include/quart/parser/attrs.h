#pragma once

#define ATTR(n) Attribute parse_##n##_attribute(Parser& parser)
#define ATTR_HANDLER(n) AttributeHandler::Result handle_##n##_attribute(Parser& parser, const Attribute& attr)

#define SIMPLE_ATTR(n, t) ATTR(n) { (void)parser; return t; }

#include <llvm/ADT/Any.h>

#include <vector>
#include <string>
#include <map>

namespace quart {

class Parser;

namespace ast {
    class Expr;
}

struct Attribute {
    enum Type {
        Invalid = -1,
        Noreturn,
        LLVMIntrinsic,
        Packed,
        Link
    };

    Type type;

    llvm::Any value;
    ast::Expr* expr;

    Attribute() = default;
    Attribute(Type type) : type(type) {}
    Attribute(Type type, llvm::Any value) : type(type), value(value) {}
    Attribute(Type type, ast::Expr* expr) : type(type), expr(expr) {}

    template<typename T> T as() const { return llvm::any_cast<T>(value); }
};

class Attributes {
public:
    static void init(Parser& parser);
};

class AttributeHandler {
public:
    enum Result {
        Ok,
        Skip
    };

    static Result handle(Parser& parser, const Attribute& attr);
};

}