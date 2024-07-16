#pragma once

#include <quart/bytecode/instruction.h>
#include <quart/language/state.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

namespace quart::codegen {

struct LocalScope {
public:
    LocalScope() = default;
    LocalScope(Function* function, size_t local_count) : m_function(function), m_local_count(local_count) {
        m_locals.reserve(local_count);
    }

    Function* function() const { return m_function; }
    size_t local_count() const { return m_local_count; }

    llvm::AllocaInst* local(size_t index) const { return m_locals[index]; }
    void set_local(size_t index, llvm::AllocaInst* local) { m_locals[index] = local; }

private:
    Function* m_function = nullptr;
    size_t m_local_count = 0;

    Vector<llvm::AllocaInst*> m_locals;
};

class LLVMCodeGen {
public:
    LLVMCodeGen(State&, String module_name);
    
    void generate();
    void generate(bytecode::Instruction*);

    llvm::Module& module() { return *m_module; }

private:
    llvm::Value* valueof(bytecode::Register);
    llvm::Value* valueof(bytecode::Operand);

    llvm::BasicBlock* create_block_from(bytecode::BasicBlock*);
    
    void set_register(bytecode::Register, llvm::Value*);

#define Op(x) void generate(bytecode::x*); // NOLINT
    ENUMERATE_BYTECODE_INSTRUCTIONS(Op)
#undef Op

    State& m_state;

    LocalScope* m_local_scope = nullptr;
    HashMap<Function*, LocalScope> m_local_scopes;

    OwnPtr<llvm::LLVMContext> m_context;
    OwnPtr<llvm::Module> m_module;
    OwnPtr<llvm::IRBuilder<>> m_ir_builder;

    Vector<llvm::Value*> m_registers;

    HashMap<bytecode::BasicBlock*, llvm::BasicBlock*> m_basic_blocks;
    HashMap<Function*, llvm::Function*> m_functions;
};

}