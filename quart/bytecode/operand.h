#pragma once

#include <quart/common.h>
#include <quart/language/types.h>

namespace quart::bytecode {

class Operand {
public:
    enum Type {
        None = 0,
        Register,
        Immediate
    };

    Operand() = default;

    explicit Operand(u64 value, quart::Type* type) : m_type(Immediate), m_value(value), m_value_type(type) {}
    explicit Operand(class Register reg);

    bool is_none() const { return m_type == None; }
    bool is_immediate() const { return m_type == Immediate; }
    bool is_register() const { return m_type == Register; }

    quart::Type* value_type() const { return m_value_type; }

    Type type() const { return m_type; }
    u64 value() const { return m_value; }

private:
    Type m_type = None;
    u64 m_value = 0;

    quart::Type* m_value_type = nullptr;
};

}