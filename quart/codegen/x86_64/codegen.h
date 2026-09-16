#pragma once

#include <quart/codegen/codegen.h>
#include <quart/bytecode/instruction.h>

#include <quart/codegen/x86_64/functions.h>
#include <quart/codegen/x86_64/registers.h>
#include <quart/codegen/x86_64/cpu.h>

#include <stack>

namespace quart::x86_64 {

class x86_64CodeGen : public CodeGen {
public:
    x86_64CodeGen(State&, String module);
    
    ErrorOr<void> generate(CompilerOptions const&) override;

private:
    void reset_all_registers();

    Register pop_reg();
    void push_reg(Register);

    String normalize(String qualified_name);

    void generate(bytecode::BasicBlock*);
    void generate(bytecode::Instruction*);

    Register generate_binary_op(
        BinaryInstruction instruction,
        bytecode::Value* lhs,
        bytecode::Value* rhs
    );

    Register generate_binary_op_with_dst(
        BinaryInstruction instruction,
        bytecode::Instruction* dst,
        bytecode::Value* lhs,
        bytecode::Value* rhs
    );

    void generate_condition(
        ConditionCode cc,
        bytecode::Instruction* instruction,
        bytecode::Value* lhs,
        bytecode::Value* rhs
    );

    Register generate_memory_access(bytecode::Value* src, bytecode::Value* index, bool ref = false);

    Register value_to_reg(bytecode::Value*, Optional<Register> dst = {});

    void save(bytecode::Value* value, Register dst);

#define Op(x) void generate(bytecode::x*); // NOLINT
    ENUMERATE_BYTECODE_INSTRUCTIONS(Op)
#undef Op

    State& m_state;
    String m_module;

    HashMap<Function*, RefPtr<CodeGenFunction>> m_functions;
    Vector<Function*> m_extern_functions;

    RefPtr<CodeGenFunction> m_current_function;
    bytecode::BasicBlock* m_current_block = nullptr;

    ConditionCode m_next_cc = ConditionCode::None;

    Function* m_next_call = nullptr;
    std::stack<Function*> m_next_calls;

    std::stack<Register> m_available_registers;
    std::stack<Register> m_callee_saved_registers;

    HashMap<bytecode::Value*, Register> m_register_map;

    Vector<String> m_strings;
};

}