#pragma once

#include <quart/common.h>

namespace quart {

template<typename T>
class TemporaryChange {
public:
    NO_COPY(TemporaryChange)
    NO_MOVE(TemporaryChange)

    TemporaryChange(T& value, T new_value) : m_value(value), m_old_value(value) {
        m_value = new_value;
    }

    ~TemporaryChange() {
        m_value = m_old_value;
    }

private:
    T& m_value;
    T m_old_value;
};

}