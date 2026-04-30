#pragma once

#include <quart/bytecode/basic_block.h>
#include <quart/bytecode/instruction.h>

namespace quart::bytecode {

class RegisterUse {
public:
    RegisterUse() = default;

    Vector<Instruction*> const& all_references() const { return m_references; }
    void add(Instruction* instruction) { m_references.push_back(instruction); }

    template<typename T> requires(std::is_base_of_v<Instruction, T>)
    T* get() {
        for (auto& ref : m_references) {
            if (T::classof(ref)) {
                return static_cast<T*>(ref);
            }
        }

        return nullptr;
    }

    template<typename T> requires(std::is_base_of_v<Instruction, T>)
    bool contains() {
        return this->get<T>() ? true : false;
    }

private:
    Vector<Instruction*> m_references;
};

class Generator {
public:
    Generator() = default;

    BasicBlock* create_block(String name = {});
    void switch_to(BasicBlock* block);

    Vector<OwnPtr<Instruction>> const& global_instructions() const { return m_global_instructions; }
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
            m_global_instructions.push_back(OwnPtr<Instruction>(op));
        }

        op->set_register_uses(*this);
        return op;
    }

    RegisterUse& register_uses(Register reg) { return m_register_uses[reg]; }
    HashMap<Register, RegisterUse> const& all_register_uses() const { return m_register_uses; }

private:
    BasicBlock* m_current_block = nullptr;

    Vector<OwnPtr<Instruction>> m_global_instructions;
    Vector<OwnPtr<BasicBlock>> m_blocks;
    
    u32 m_next_block_id = 0;
    u32 m_next_register_id = 0;

    HashMap<Register, RegisterUse> m_register_uses;
};

}