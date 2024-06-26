#pragma once

#include <quart/bytecode/instruction.h>
#include <quart/language/state.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

namespace quart::codegen {

class LLVMCodeGen {
public:
    LLVMCodeGen(State&, String module_name);
    
    void generate(bytecode::Instruction*);

private:
    llvm::Value* valueof(bytecode::Operand);
    void set_register(bytecode::Register, llvm::Value*);

#define Op(x) void generate(bytecode::x*); // NOLINT
    ENUMERATE_BYTECODE_INSTRUCTIONS(Op)
#undef Op

    State& m_state;

    OwnPtr<llvm::LLVMContext> m_context;
    OwnPtr<llvm::Module> m_module;
    OwnPtr<llvm::IRBuilder<>> m_ir_builder;

    Vector<llvm::Value*> m_registers;
};

}