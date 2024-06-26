#pragma once

#include <quart/bytecode/basic_block.h>
#include <quart/bytecode/instruction.h>

namespace quart::bytecode {

class Generator {
public:
    Generator() = default;

    BasicBlock& create_block(std::string name = {});
    void switch_to(BasicBlock& block);

    bytecode::Register allocate_register();
    size_t register_count() const { return m_next_register_id; }

    template<typename T, typename... Args>
    T* emit(Args&&... args) {
        T* op = new T(std::forward<Args>(args)...);
        m_current_block->add_instruction(op);

        return op;
    }

private:
    BasicBlock* m_current_block = nullptr;

    Vector<OwnPtr<BasicBlock>> m_blocks;
    
    u32 m_next_block_id = 0;
    u32 m_next_register_id = 1; // 0 is reserved for the accumulator
};

}