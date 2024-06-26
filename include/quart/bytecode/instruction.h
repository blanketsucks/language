#pragma once

#include <quart/common.h>

#include <quart/bytecode/operand.h>
#include <quart/bytecode/register.h>

#include <string>
#include <vector>

#define ENUMERATE_BYTECODE_INSTRUCTIONS(Op)         \
    Op(Move)                                        \
    Op(NewString)                                   \
    Op(NewArray)                                    \
    Op(GetLocal)                                    \
    Op(GetLocalRef)                                 \
    Op(SetLocal)                                    \
    Op(Read)                                        \
    Op(Write)                                       \
    Op(Add)                                         \
    Op(Sub)                                         \
    Op(Mul)                                         \
    Op(Div)                                         \
    Op(Mod)                                         \
    Op(NewFunction)                                 \
    Op(GetFunction)                                 \
    Op(Jump)                                        \
    Op(JumpIf)                                      \
    Op(Cast)

namespace quart::bytecode {

class BasicBlock;

class Instruction {
public:
    NO_COPY(Instruction)
    DEFAULT_MOVE(Instruction)

    static bool classof(Instruction const*) { return true; }

    virtual ~Instruction() = default;

    enum InstructionType {
    #define Op(x) x, // NOLINT
        ENUMERATE_BYTECODE_INSTRUCTIONS(Op)
    #undef Op
    };

    InstructionType type() const { return m_type; }

    template<typename T> requires(std::is_base_of_v<Instruction, T>)
    bool is() const {
        return T::classof(this);
    }

    template<typename T> requires(std::is_base_of_v<Instruction, T>)
    [[nodiscard]] T const* as() const {
        return T::classof(this) ? static_cast<T const*>(this) : nullptr;
    }

protected:
    Instruction(InstructionType type) : m_type(type) {}

private:
    InstructionType m_type;
};

template<Instruction::InstructionType Ty>
class InstructionBase : public Instruction {
public:
    static bool classof(Instruction const* inst) { return inst->type() == Ty; }
    InstructionBase() : Instruction(Ty) {}
};

// `dst = src`
class Move : public InstructionBase<Instruction::Move> {
public:
    Move(Register dst, Operand src) : m_dst(dst), m_src(src) {}

    Register dst() const { return m_dst; }
    Operand src() const { return m_src; }

private:
    Register m_dst;
    Operand m_src;
};

// `dst = "value"`
class NewString : public InstructionBase<Instruction::NewString> {
public:
    NewString(Register dst, String value) : m_dst(dst), m_value(move(value)) {}

    Register dst() const { return m_dst; }
    String const& value() const { return m_value; }

private:
    Register m_dst;
    String m_value;
};

// `dst = [elements...]`
class NewArray : public InstructionBase<Instruction::NewArray> {
public:
    NewArray(Register dst, Vector<Operand> elements) : m_dst(dst), m_elements(move(elements)) {}

    Register dst() const { return m_dst; }
    Vector<Operand> const& elements() const { return m_elements; }

private:
    Register m_dst;
    Vector<Operand> m_elements;
};

// `dst = locals[index]`
class GetLocal : public InstructionBase<Instruction::GetLocal> {
public:
    GetLocal(Register dst, u32 index) : m_dst(dst), m_index(index) {}

    Register dst() const { return m_dst; }
    u32 index() const { return m_index; }

private:
    Register m_dst;
    u32 m_index;
};

// `dst = &locals[index]`
class GetLocalRef : public InstructionBase<Instruction::GetLocalRef> {
public:
    GetLocalRef(Register dst, u32 index) : m_dst(dst), m_index(index) {}

    Register dst() const { return m_dst; }
    u32 index() const { return m_index; }

private:
    Register m_dst;
    u32 m_index;
};

// `locals[index] = src`
class SetLocal : public InstructionBase<Instruction::SetLocal> {
public:
    SetLocal(u32 index, Operand src) : m_index(index), m_src(src) {}

    u32 index() const { return m_index; }
    Operand src() const { return m_src; }

private:
    u32 m_index;
    Operand m_src;
};

// `dst = *src`
class Read : public InstructionBase<Instruction::Read> {
public:
    Read(Register dst, Register src) : m_dst(dst), m_src(src) {}

    Register dst() const { return m_dst; }
    Register src() const { return m_src; }

private:
    Register m_dst;
    Register m_src;
};

// `*dst = src`
class Write : public InstructionBase<Instruction::Write> {
public:
    Write(Register dst, Operand src) : m_dst(dst), m_src(src) {}

    Register dst() const { return m_dst; }
    Operand src() const { return m_src; }

private:
    Register m_dst;
    Operand m_src;
};


// NOLINTNEXTLINE
#define DEFINE_ARITHMETIC_INSTRUCTION(name)                                                                             \
    class name : public InstructionBase<Instruction::name> { /* NOLINT */                                               \
    public:                                                                                                             \
        name(Register dst, Operand lhs, Operand rhs) : m_dst(dst), m_lhs(lhs), m_rhs(rhs) {}                            \
                                                                                                                        \
        Register dst() const { return m_dst; }                                                                          \
        Operand lhs() const { return m_lhs; }                                                                           \
        Operand rhs() const { return m_rhs; }                                                                           \
                                                                                                                        \
    private:                                                                                                            \
        Register m_dst;                                                                                                 \
        Operand m_lhs;                                                                                                  \
        Operand m_rhs;                                                                                                  \
    };

DEFINE_ARITHMETIC_INSTRUCTION(Add)
DEFINE_ARITHMETIC_INSTRUCTION(Sub)
DEFINE_ARITHMETIC_INSTRUCTION(Mul)
DEFINE_ARITHMETIC_INSTRUCTION(Div)
DEFINE_ARITHMETIC_INSTRUCTION(Mod)

// `goto target`
class Jump : public InstructionBase<Instruction::Jump> {
public:
    Jump(BasicBlock* target) : m_target(target) {}

    BasicBlock* target() const { return m_target; }

private:
    BasicBlock* m_target;
};

// `if (condition) { goto true_target } else { goto false_target }`
class JumpIf : public InstructionBase<Instruction::JumpIf> {
public:
    JumpIf(
        Operand condition, BasicBlock* true_target, BasicBlock* false_target
    ) : m_condition(condition), m_true_target(true_target), m_false_target(false_target) {}

    Operand condition() const { return m_condition; }

    BasicBlock* true_target() const { return m_true_target; }
    BasicBlock* false_target() const { return m_false_target; }

private:
    Operand m_condition;
    
    BasicBlock* m_true_target;
    BasicBlock* m_false_target;
};

class NewFunction : public InstructionBase<Instruction::NewFunction> {
public:
    NewFunction(String name, BasicBlock* entry) : m_name(move(name)), m_entry(entry) {}

    String const& name() const { return m_name; }
    BasicBlock* entry() const { return m_entry; }

private:
    String m_name;
    BasicBlock* m_entry;
};

class GetFunction : public InstructionBase<Instruction::GetFunction> {
public:
    GetFunction(Register dst, String name) : m_dst(dst), m_name(move(name)) {}

    Register dst() const { return m_dst; }
    String const& name() const { return m_name; }

private:
    Register m_dst;
    String m_name;
};

class Cast : public InstructionBase<Instruction::Cast> {
public:
    Cast(Register dst, Operand src, Type* type) : m_dst(dst), m_src(src), m_type(type) {}

    Register dst() const { return m_dst; }
    Operand src() const { return m_src; }
    Type* type() const { return m_type; }

private:
    Register m_dst;
    Operand m_src;
    
    Type* m_type;
};

}