#include <quart/bytecode/operand.h>
#include <quart/bytecode/register.h>

namespace quart::bytecode {

Operand::Operand(class Register reg) : m_type(Register), m_value(reg.index()) {}

bytecode::Register Operand::as_reg() const {
    ASSERT(is_register(), {});
    return bytecode::Register(m_value);
}

}