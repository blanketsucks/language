#pragma once

#include <quart/common.h>

#include <quart/bytecode/register.h>
#include <quart/language/types.h>

#include <string>
#include <vector>

#define ENUMERATE_BYTECODE_INSTRUCTIONS(Op)         \
    Op(Move)                                        \
    Op(NewString)                                   \
    Op(NewArray)                                    \
    Op(NewLocalScope)                               \
    Op(GetLocal)                                    \
    Op(GetLocalRef)                                 \
    Op(SetLocal)                                    \
    Op(GetGlobal)                                   \
    Op(GetGlobalRef)                                \
    Op(SetGlobal)                                   \
    Op(GetMember)                                   \
    Op(SetMember)                                   \
    Op(GetMemberRef)                                \
    Op(Read)                                        \
    Op(Write)                                       \
    Op(Add)                                         \
    Op(Sub)                                         \
    Op(Mul)                                         \
    Op(Div)                                         \
    Op(Mod)                                         \
    Op(Or)                                          \
    Op(And)                                         \
    Op(LogicalOr)                                   \
    Op(LogicalAnd)                                  \
    Op(Xor)                                         \
    Op(Rsh)                                         \
    Op(Lsh)                                         \
    Op(Eq)                                          \
    Op(Neq)                                         \
    Op(Gt)                                          \
    Op(Lt)                                          \
    Op(Gte)                                         \
    Op(Lte)                                         \
    Op(NewFunction)                                 \
    Op(GetFunction)                                 \
    Op(Return)                                      \
    Op(Call)                                        \
    Op(Jump)                                        \
    Op(JumpIf)                                      \
    Op(Cast)                                        \
    Op(NewStruct)                                   \
    Op(Construct)                                   \
    Op(Alloca)                                      \
    Op(NewTuple)                                    \
    Op(Null)                                        \
    Op(Boolean)                                     \
    Op(Not)                                         \
    Op(Memcpy)                                      \
    Op(GetReturn)                                   \

namespace quart {
    class Function;
    class Struct;
    class Constant;
}

namespace quart::bytecode {

class BasicBlock;

class Operand {
public:
    enum class Type : u8 {
        Register,
        Value
    };

    Operand() = delete;

    Operand(Register reg) : m_value(reg.index()), m_type(Type::Register) {}
    Operand(u64 value, quart::Type* type) : m_value(value), m_value_type(type), m_type(Type::Value) {}

    bool is_register() const { return m_type == Type::Register; }
    bool is_value() const { return m_type == Type::Value; }

    Register reg() const { return Register(m_value); }
    u64 value() const { return m_value; }

    quart::Type* value_type() const { return m_value_type; }

private:
    u64 m_value;
    quart::Type* m_value_type = nullptr;

    Type m_type;
};

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

    virtual bool is_terminator() const { return false; }

    StringView type_name() const {
        switch (m_type) {
        #define Op(x) case x: return #x; // NOLINT
            ENUMERATE_BYTECODE_INSTRUCTIONS(Op)
        #undef Op
        }

        return {};
    }

    virtual void dump() const = 0;

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

class Move : public InstructionBase<Instruction::Move> {
public:
    Move(Register dst, u64 src) : m_dst(dst), m_src(src) {}

    Register dst() const { return m_dst; }
    u64 src() const { return m_src; }

    void dump() const override;

private:
    Register m_dst;
    u64 m_src;
};

class NewString : public InstructionBase<Instruction::NewString> {
public:
    NewString(Register dst, String value) : m_dst(dst), m_value(move(value)) {}

    Register dst() const { return m_dst; }
    String const& value() const { return m_value; }

    void dump() const override;

private:
    Register m_dst;
    String m_value;
};

class NewArray : public InstructionBase<Instruction::NewArray> {
public:
    NewArray(Register dst, Vector<Register> elements, ArrayType* type) : m_dst(dst), m_elements(move(elements)), m_type(type) {}

    Register dst() const { return m_dst; }
    Vector<Register> const& elements() const { return m_elements; }
    ArrayType* type() const { return m_type; }

    void dump() const override;

private:
    Register m_dst;
    Vector<Register> m_elements;
    ArrayType* m_type;
};

class GetMember : public InstructionBase<Instruction::GetMember> {
public:
    GetMember(Register dst, Register src, Operand index) : m_dst(dst), m_src(src), m_index(index) {}

    Register dst() const { return m_dst; }
    Register src() const { return m_src; }
    Operand index() const { return m_index; }

    void dump() const override;

private:
    Register m_dst;
    Register m_src;
    Operand m_index;
};

class SetMember : public InstructionBase<Instruction::SetMember> {
public:
    SetMember(Register dst, Operand index, Register src) : m_dst(dst), m_index(index), m_src(src) {}

    Register dst() const { return m_dst; }
    Operand index() const { return m_index; }
    Register src() const { return m_src; }

    void dump() const override;

private:
    Register m_dst;
    Operand m_index;
    Register m_src;
};

class GetMemberRef : public InstructionBase<Instruction::GetMemberRef> {
public:
    GetMemberRef(Register dst, Register src, Operand index) : m_dst(dst), m_src(src), m_index(index) {}

    Register dst() const { return m_dst; }
    Register src() const { return m_src; }
    Operand index() const { return m_index; }

    void dump() const override;

private:
    Register m_dst;
    Register m_src;
    Operand m_index;
};

class NewLocalScope : public InstructionBase<Instruction::NewLocalScope> {
public:
    NewLocalScope(Function* function) : m_function(function) {}

    Function* function() const { return m_function; }

    void dump() const override;

private:
    Function* m_function;
};

class GetLocal : public InstructionBase<Instruction::GetLocal> {
public:
    GetLocal(Register dst, u32 index) : m_dst(dst), m_index(index) {}

    Register dst() const { return m_dst; }
    u32 index() const { return m_index; }

    void dump() const override;

private:
    Register m_dst;
    u32 m_index;
};

class GetLocalRef : public InstructionBase<Instruction::GetLocalRef> {
public:
    GetLocalRef(Register dst, u32 index) : m_dst(dst), m_index(index) {}

    Register dst() const { return m_dst; }
    u32 index() const { return m_index; }

    void dump() const override;

private:
    Register m_dst;
    u32 m_index;
};

class SetLocal : public InstructionBase<Instruction::SetLocal> {
public:
    SetLocal(u32 index, Optional<Register> src) : m_index(index), m_src(src) {}

    u32 index() const { return m_index; }
    Optional<Register> src() const { return m_src; }

    void dump() const override;

private:
    u32 m_index;
    Optional<Register> m_src;
};

class GetGlobal : public InstructionBase<Instruction::GetGlobal> {
public:
    GetGlobal(Register dst, u32 index) : m_dst(dst), m_index(index) {}

    Register dst() const { return m_dst; }
    u32 index() const { return m_index; }

    void dump() const override;

private:
    Register m_dst;
    u32 m_index;
};

class GetGlobalRef : public InstructionBase<Instruction::GetGlobalRef> {
public:
    GetGlobalRef(Register dst, u32 index) : m_dst(dst), m_index(index) {}

    Register dst() const { return m_dst; }
    u32 index() const { return m_index; }

    void dump() const override;

private:
    Register m_dst;
    u32 m_index;
};

class SetGlobal : public InstructionBase<Instruction::SetGlobal> {
public:
    SetGlobal(u32 index, Constant* src) : m_index(index), m_src(src) {}

    u32 index() const { return m_index; }
    Constant* src() const { return m_src; }

    void dump() const override;

private:
    u32 m_index;
    Constant* m_src;
};

class Read : public InstructionBase<Instruction::Read> {
public:
    Read(Register dst, Register src) : m_dst(dst), m_src(src) {}

    Register dst() const { return m_dst; }
    Register src() const { return m_src; }

    void dump() const override;

private:
    Register m_dst;
    Register m_src;
};

class Write : public InstructionBase<Instruction::Write> {
public:
    Write(Register dst, Register src) : m_dst(dst), m_src(src) {}

    Register dst() const { return m_dst; }
    Register src() const { return m_src; }

    void dump() const override;

private:
    Register m_dst;
    Register m_src;
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
        void dump() const override;                                                                                     \
    private:                                                                                                            \
        Register m_dst;                                                                                                 \
        Operand m_lhs;                                                                                                  \
        Operand m_rhs;                                                                                                  \
    };

ENUMERATE_BINARY_OPS(DEFINE_ARITHMETIC_INSTRUCTION)

#undef DEFINE_ARITHMETIC_INSTRUCTION

// `goto target`
class Jump : public InstructionBase<Instruction::Jump> {
public:
    Jump(BasicBlock* target) : m_target(target) {}

    BasicBlock* target() const { return m_target; }

    bool is_terminator() const override { return true; }
    void dump() const override;

private:
    BasicBlock* m_target;
};

// `if (condition) { goto true_target } else { goto false_target }`
class JumpIf : public InstructionBase<Instruction::JumpIf> {
public:
    JumpIf(
        Register condition, BasicBlock* true_target, BasicBlock* false_target
    ) : m_condition(condition), m_true_target(true_target), m_false_target(false_target) {}

    Register condition() const { return m_condition; }

    BasicBlock* true_target() const { return m_true_target; }
    BasicBlock* false_target() const { return m_false_target; }

    bool is_terminator() const override { return true; }
    void dump() const override;

private:
    Register m_condition;
    
    BasicBlock* m_true_target;
    BasicBlock* m_false_target;
};

class NewFunction : public InstructionBase<Instruction::NewFunction> {
public:
    NewFunction(Function* function) : m_function(function) {}

    Function* function() const { return m_function; }

    void dump() const override;

private:
    Function* m_function;
};

class GetFunction : public InstructionBase<Instruction::GetFunction> {
public:
    GetFunction(Register dst, Function* function) : m_dst(dst), m_function(function) {}

    Register dst() const { return m_dst; }
    Function* function() const { return m_function; }

    void dump() const override;

private:
    Register m_dst;
    Function* m_function;
};

class Return : public InstructionBase<Instruction::Return> {
public:
    Return(Optional<Register> value = {}) : m_value(value) {}

    Optional<Register> value() const { return m_value; }

    bool is_terminator() const override { return true; }
    void dump() const override;

private:
    Optional<Register> m_value;
};

class Call : public InstructionBase<Instruction::Call> {
public:
    Call(
        Register dst, Register function, FunctionType const* function_type, Vector<Register> arguments
    ) : m_dst(dst), m_function(function), m_function_type(function_type), m_arguments(move(arguments)) {}

    Register dst() const { return m_dst; }
    Register function() const { return m_function; }
    FunctionType const* function_type() const { return m_function_type; }
    Vector<Register> const& arguments() const { return m_arguments; }

    void dump() const override;

private:
    Register m_dst;
    Register m_function;
    FunctionType const* m_function_type;
    Vector<Register> m_arguments;
};

class Cast : public InstructionBase<Instruction::Cast> {
public:
    Cast(Register dst, Register src, Type* type) : m_dst(dst), m_src(src), m_type(type) {}

    Register dst() const { return m_dst; }
    Register src() const { return m_src; }
    Type* type() const { return m_type; }

    void dump() const override;

private:
    Register m_dst;
    Register m_src;
    
    Type* m_type;
};

class NewStruct : public InstructionBase<Instruction::NewStruct> {
public:
    NewStruct(Struct* structure) : m_structure(structure) {}

    Struct* structure() const { return m_structure; }

    void dump() const override;

private:
    Struct* m_structure;
};

class Construct : public InstructionBase<Instruction::Construct> {
public:
    Construct(Register dst, Struct* structure, Vector<Register> arguments) : m_dst(dst), m_structure(structure), m_arguments(move(arguments)) {}

    Register dst() const { return m_dst; }
    Struct* structure() const { return m_structure; }
    Vector<Register> const& arguments() const { return m_arguments; }

    void dump() const override;

private:
    Register m_dst;
    Struct* m_structure;
    Vector<Register> m_arguments;
};

class Alloca : public InstructionBase<Instruction::Alloca> {
public:
    Alloca(Register dst, Type* type) : m_dst(dst), m_type(type) {}

    Register dst() const { return m_dst; }
    Type* type() const { return m_type; }

    void dump() const override;

private:
    Register m_dst;
    Type* m_type;
};

class NewTuple : public InstructionBase<Instruction::NewTuple> {
public:
    NewTuple(Register dst, TupleType* type, Vector<Register> elements) : m_dst(dst), m_type(type), m_elements(move(elements)) {}

    Register dst() const { return m_dst; }
    TupleType* type() const { return m_type; }
    Vector<Register> const& elements() const { return m_elements; }

    void dump() const override {}

private:
    Register m_dst;
    TupleType* m_type;
    Vector<Register> m_elements;
};

class Null : public InstructionBase<Instruction::Null> {
public:
    Null(Register dst, Type* type) : m_dst(dst), m_type(type) {}

    Register dst() const { return m_dst; }
    Type* type() const { return m_type; }

    void dump() const override;

private:
    Register m_dst;
    Type* m_type;
};

class Boolean : public InstructionBase<Instruction::Boolean> {
public:
    Boolean(Register dst, bool value) : m_dst(dst), m_value(value) {}

    Register dst() const { return m_dst; }
    bool value() const { return m_value; }

    void dump() const override;

private:
    Register m_dst;
    bool m_value;
};

class Not : public InstructionBase<Instruction::Boolean> {
public:
    Not(Register dst, Register src) : m_dst(dst), m_src(src) {}

    Register dst() const { return m_dst; }
    Register src() const { return m_src; }

    void dump() const override;

private:
    Register m_dst;
    Register m_src;
};

class Memcpy : public InstructionBase<Instruction::Memcpy> {
public:
    Memcpy(Register dst, Register src, size_t size) : m_dst(dst), m_src(src), m_size(size) {}

    Register dst() const { return m_dst; }
    Register src() const { return m_src; }
    size_t size() const { return m_size; }

    void dump() const override;

private:
    Register m_dst;
    Register m_src;
    size_t m_size;
};

class GetReturn : public InstructionBase<Instruction::GetReturn> {
public:
    GetReturn(Register dst) : m_dst(dst) {}

    Register dst() const { return m_dst; }

    void dump() const override;
private:
    Register m_dst;
};

}