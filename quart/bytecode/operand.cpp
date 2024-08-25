#include <quart/bytecode/operand.h>
#include <quart/bytecode/register.h>

namespace quart::bytecode {

Operand::Operand(class Register reg) : m_type(Register), m_value(reg.index()) {}

}