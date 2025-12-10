#pragma once

#include <quart/bytecode/instruction.h>
#include <quart/language/state.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

namespace quart {
    struct CompilerOptions;
}

namespace quart::codegen {

struct Local {
    llvm::Value* store;
    llvm::Type* type;

    bool needs_store() const { return store == nullptr && type; }
};

struct LocalScope {
public:
    LocalScope() = default;
    LocalScope(Function* function, size_t local_count) : m_function(function), m_local_count(local_count) {
        m_locals.reserve(local_count);
    }

    Function* function() const { return m_function; }
    size_t local_count() const { return m_local_count; }

    void set_return(llvm::Value* value) { m_return = value; }
    llvm::Value* return_value() const { return m_return; }

    Local const& local(size_t index) const { return m_locals[index]; }
    Local& local(size_t index) { return m_locals[index]; }

    void set_local(size_t index, llvm::Value* store, llvm::Type* type) { m_locals[index] = { store, type }; }

private:
    Function* m_function = nullptr;
    size_t m_local_count = 0;

    Vector<Local> m_locals;
    llvm::Value* m_return = nullptr;
};

class LLVMCodeGen {
public:
    LLVMCodeGen(State&, String module_name);
    
    ErrorOr<void> generate(CompilerOptions const&);
    
    void generate(bytecode::BasicBlock*);
    void generate(bytecode::Instruction*);

    llvm::Module& module() { return *m_module; }

private:
    llvm::Value* valueof(bytecode::Register);
    llvm::Value* valueof(bytecode::Operand const&);

    llvm::Value* valueof(Constant*);

    // Turns :: into . to make the LLVM IR cleaner
    String normalize(String qualifed_name);

    llvm::BasicBlock* create_block_from(bytecode::BasicBlock*);

    llvm::Value* create_gep(bytecode::Register src, bytecode::Operand index);
    
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
    Vector<llvm::GlobalVariable*> m_globals;

    HashMap<TupleType*, llvm::StructType*> m_tuple_types;
    size_t m_tuple_count = 0;

    HashMap<bytecode::BasicBlock*, llvm::BasicBlock*> m_basic_blocks;
    HashMap<Function*, llvm::Function*> m_functions;
    HashMap<Struct*, llvm::StructType*> m_structs;
};

}