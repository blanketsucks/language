#pragma once

#include <quart/common.h>

#include <string>
#include <vector>

namespace quart::bytecode {

class Instruction;

class BasicBlock {
public:
    static OwnPtr<BasicBlock> create(String name) {
        return OwnPtr<BasicBlock>(new BasicBlock(move(name)));
    }

    String const& name() const { return m_name; }

    Vector<Instruction*>& instructions() { return m_instructions; }
    Vector<Instruction*> const& instructions() const { return m_instructions; }

    void add_instruction(Instruction*);

    bool is_terminated() const { return m_terminated; }
    void terminate() { m_terminated = true; }

private:
    explicit BasicBlock(String name) : m_name(move(name)) {}

    String m_name;
    Vector<Instruction*> m_instructions;

    bool m_terminated = false;
};

}