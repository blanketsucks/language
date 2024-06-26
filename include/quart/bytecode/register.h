#pragma once

#include <quart/common.h>

namespace quart::bytecode {

class Register {
public:
    constexpr explicit Register(u32 index) : m_index(index) {}

    static constexpr Register accumulator() {
        return Register(0);
    }

    constexpr bool operator==(const Register& other) const {
        return m_index == other.m_index;
    }

    constexpr u32 index() const { return m_index; }

private:
    u32 m_index = 0;
};

}