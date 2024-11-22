#pragma once

#include <quart/common.h>

#include <llvm/ADT/Any.h>

#include <vector>
#include <string>
#include <map>

namespace quart {

class Parser;

namespace ast {
    class Expr;
}

struct LinkInfo {
    String name;
    String arch; 
    String section; 
    String platform;

    LinkInfo() = default;
    LinkInfo(HashMap<String, String>& values) {
        name = move(values["name"]);
        arch = move(values["arch"]);
        section = move(values["section"]);
        platform = move(values["platform"]);
    }

    bool has_arch() const { return !arch.empty(); }
    bool has_platform() const { return !section.empty(); }
};

class Attribute {
public:
    enum Type {
        None = 0,
        Noreturn,
        Packed,
        Link
    };

    Attribute() = default;
    Attribute(Type type, llvm::Any value = {}) : m_type(type), m_value(move(value)) {}

    Type type() const { return m_type; }

    template<typename T> T value() const {
        return llvm::any_cast<T>(m_value);
    }

private:
    Type m_type = None;
    llvm::Any m_value;
};

class Attributes {
public:
    static void init(Parser&);
};

class AttributeHandler {
public:
    enum Result {
        Ok,
        Skip
    };

    static Result handle(Parser&, const Attribute& attr);
};

}