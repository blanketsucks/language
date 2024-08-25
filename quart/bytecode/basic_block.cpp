#include <quart/bytecode/basic_block.h>
#include <quart/bytecode/instruction.h>

namespace quart::bytecode {

void BasicBlock::add_instruction(Instruction* inst) {
    m_instructions.push_back(inst);
    if (inst->is_terminator()) {
        m_terminated = true;
    }
}

}