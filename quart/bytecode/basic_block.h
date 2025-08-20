#pragma once

#include <quart/common.h>

#include <string>
#include <vector>

namespace quart::bytecode {

class Instruction;

class BasicBlock {
public:
    static OwnPtr<BasicBlock> create(String name);

    String const& name() const { return m_name; }

    Vector<OwnPtr<Instruction>>& instructions() { return m_instructions; }
    Vector<OwnPtr<Instruction>> const& instructions() const { return m_instructions; }

    void add_instruction(Instruction*);

    bool is_terminated() const { return m_terminated; }
    void terminate() { m_terminated = true; }

    void dump() const;

private:
    explicit BasicBlock(String name);

    String m_name;
    Vector<OwnPtr<Instruction>> m_instructions;

    bool m_terminated = false;
};

}