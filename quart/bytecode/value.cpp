#include <quart/bytecode/value.h>
#include <quart/format.h>
#include <iostream>

namespace quart::bytecode {

static size_t s_name_id = 0;

Value::Value(Type* type, ValueID id) : m_type(type), m_id(id) {
    m_name = format("{}", s_name_id++);
}

void Value::dump() const {
    auto& stream = std::cout;
    
    this->print(stream);
    stream << '\n';
}

}