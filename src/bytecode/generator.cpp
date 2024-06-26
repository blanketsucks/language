#include <quart/bytecode/generator.h>
#include <quart/format.h>

namespace quart::bytecode {

bytecode::Register Generator::allocate_register() {
    return bytecode::Register(m_next_register_id++);
}

BasicBlock& Generator::create_block(std::string name) {
    if (name.empty()) {
        name = format("block.{0}", m_next_block_id++);
    }

    m_blocks.push_back(BasicBlock::create(std::move(name)));
    return *m_blocks.back();
}

void Generator::switch_to(BasicBlock& block) {
    m_current_block = &block;
}

}
