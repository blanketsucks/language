#include <quart/codegen/llvm.h>

namespace quart::codegen {

LLVMCodeGen::LLVMCodeGen(State& state, String module_name) : m_state(state) {
    m_context = make<llvm::LLVMContext>();
    m_module = make<llvm::Module>(move(module_name), *m_context);
    m_ir_builder = make<llvm::IRBuilder<>>(*m_context);

    m_registers.reserve(m_state.register_count());
}

void LLVMCodeGen::generate(bytecode::Move* inst) {
    llvm::Value* src = valueof(inst->src());
    this->set_register(inst->dst(), src);
}

void LLVMCodeGen::generate(bytecode::NewString* inst) {
    llvm::Value* value = m_ir_builder->CreateGlobalStringPtr(inst->value(), ".str", 0, &*m_module);
    this->set_register(inst->dst(), value);
}

#define GENERIC_ARITH_INST(Name) /* NOLINT */                   \
    void LLVMCodeGen::generate(bytecode::Name* inst) {          \
        llvm::Value* lhs = valueof(inst->lhs());                \
        llvm::Value* rhs = valueof(inst->rhs());                \
                                                                \
        llvm::Value* value = nullptr;                           \
        if (lhs->getType()->isFloatingPointTy()) {              \
            value = m_ir_builder->CreateF##Name(lhs, rhs);      \
        } else {                                                \
            value = m_ir_builder->Create##Name(lhs, rhs);       \
        }                                                       \
                                                                \
        this->set_register(inst->dst(), value);                 \
    }

#define SIGNED_ARITH_INST(Name, Function) /* NOLINT */                  \
    void LLVMCodeGen::generate(bytecode::Name* inst) {                  \
        llvm::Value* lhs = valueof(inst->lhs());                        \
        llvm::Value* rhs = valueof(inst->rhs());                        \
                                                                        \
        llvm::Value* value = nullptr;                                   \
        quart::Type* type = m_state.type(inst->lhs());                  \
                                                                        \
        if (type->is_float()) {                                         \
            value = m_ir_builder->CreateF##Function(lhs, rhs);          \
        } else if (type->is_int_unsigned()) {                           \
            value = m_ir_builder->CreateU##Function(lhs, rhs);          \
        } else {                                                        \
            value = m_ir_builder->CreateS##Function(lhs, rhs);          \
        }                                                               \
                                                                        \
        this->set_register(inst->dst(), value);                         \
    }

GENERIC_ARITH_INST(Add)
GENERIC_ARITH_INST(Sub)
GENERIC_ARITH_INST(Mul)

SIGNED_ARITH_INST(Div, Div)
SIGNED_ARITH_INST(Mod, Rem)

void LLVMCodeGen::generate(bytecode::GetLocal*) {}
void LLVMCodeGen::generate(bytecode::GetLocalRef*) {}
void LLVMCodeGen::generate(bytecode::SetLocal*) {}

void LLVMCodeGen::generate(bytecode::Read*) {}
void LLVMCodeGen::generate(bytecode::Write*) {}

void LLVMCodeGen::generate(bytecode::Jump*) {}
void LLVMCodeGen::generate(bytecode::JumpIf*) {}

void LLVMCodeGen::generate(bytecode::NewFunction*) {}
void LLVMCodeGen::generate(bytecode::GetFunction*) {}

void LLVMCodeGen::generate(bytecode::Cast*) {}

void LLVMCodeGen::generate(bytecode::NewArray*) {}

void LLVMCodeGen::set_register(bytecode::Register reg, llvm::Value* value) {
    m_registers[reg.index()] = value;
}

llvm::Value* LLVMCodeGen::valueof(bytecode::Operand operand) {
    if (operand.is_register()) {
        return m_registers[operand.value()];
    }

    llvm::Type* type = operand.value_type()->to_llvm_type(*m_context);
    return llvm::ConstantInt::get(type, operand.value());
}

void LLVMCodeGen::generate(bytecode::Instruction* inst) {
    switch (inst->type()) {
    #define Op(x) /* NOLINT */                                      \
        case bytecode::Instruction::x:                              \
            return this->generate(static_cast<bytecode::x*>(inst)); \

        ENUMERATE_BYTECODE_INSTRUCTIONS(Op)
    #undef Op
    }
}

}