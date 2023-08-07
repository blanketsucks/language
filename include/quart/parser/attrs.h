#pragma once

#define ATTR(n) Attribute handle_##n##_attribute(Parser& parser)

#define SIMPLE_ATTR(n, t) ATTR(n) { (void)parser; return t; }

#include <quart/utils/pointer.h>

#include <llvm/ADT/Any.h>

#include <vector>
#include <string>
#include <map>

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
    utils::Ref<ast::Expr> expr;

    Attribute() = default;
    Attribute(Type type) : type(type) {}
    Attribute(Type type, llvm::Any value) : type(type), value(value) {}
    Attribute(Type type, utils::Ref<ast::Expr> expr) : type(type), expr(expr) {}

    template<typename T> T as() { return llvm::any_cast<T>(value); }
};

class Attributes {
public:
    static void init(Parser& parser);
};
