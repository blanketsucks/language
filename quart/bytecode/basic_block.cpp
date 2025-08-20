#include <quart/bytecode/basic_block.h>
#include <quart/bytecode/instruction.h>
#include <quart/format.h>

namespace quart::bytecode {

BasicBlock::BasicBlock(String name) : m_name(move(name)) {}

OwnPtr<BasicBlock> BasicBlock::create(String name) {
    return OwnPtr<BasicBlock>(new BasicBlock(move(name)));
}

void BasicBlock::add_instruction(Instruction* inst) {
    m_instructions.push_back(OwnPtr<Instruction>(inst));
    if (inst->is_terminator()) {
        m_terminated = true;
    }
}

void BasicBlock::dump() const {
    outln("{}:", m_name);
    for (auto& instruction : m_instructions) {
        out("  "); instruction->dump();
    }
}

}