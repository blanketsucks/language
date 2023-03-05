#ifndef _PARSER_ATTRS_H
#define _PARSER_ATTRS_H

#define ATTR(n) Attribute handle_##n##_attribute(Parser& parser)

#define SIMPLE_ATTR(n, t) ATTR(n) { (void)parser; return t; }

#include "utils/pointer.h"

#include "llvm/ADT/Any.h"

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
        Impl,
        LLVMIntrinsic,
        Packed
    };

    Type type;

    std::string value;
    utils::Ref<ast::Expr> expr;

    Attribute() = default;
    Attribute(Type type) : type(type) {}
    Attribute(Type type, std::string value) : type(type), value(value) {}
    Attribute(Type type, utils::Ref<ast::Expr> expr) : type(type), expr(expr) {}
};

class Attributes {
public:
    static void init(Parser& parser);
};

#endif