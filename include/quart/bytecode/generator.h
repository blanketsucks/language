#pragma once

#include <quart/bytecode/basic_block.h>
#include <quart/bytecode/instruction.h>

namespace quart::bytecode {

class Generator {
public:
    Generator() = default;

    BasicBlock* create_block(String name = {});
    void switch_to(BasicBlock* block);

    Vector<Instruction*> const& global_instructions() const { return m_global_instructions; }
    Vector<OwnPtr<BasicBlock>>& blocks() { return m_blocks; }

    BasicBlock* current_block() { return m_current_block; }

    bytecode::Register allocate_register();
    size_t register_count() const { return m_next_register_id; }

    template<typename T, typename... Args>
    T* emit(Args&&... args) {
        T* op = new T(std::forward<Args>(args)...);
        if (m_current_block) {
            m_current_block->add_instruction(op);
        } else {
            m_global_instructions.push_back(op);
        }

        return op;
    }

private:
    BasicBlock* m_current_block = nullptr;

    Vector<Instruction*> m_global_instructions;
    Vector<OwnPtr<BasicBlock>> m_blocks;
    
    u32 m_next_block_id = 0;
    u32 m_next_register_id = 0;
};

}