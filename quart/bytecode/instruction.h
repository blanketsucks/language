#pragma once

#include <quart/common.h>

#include <quart/bytecode/register.h>
#include <quart/bytecode/value.h>
#include <quart/bytecode/constant.h>
#include <quart/lexer/tokens.h>
#include <quart/language/types.h>

#include <string>
#include <vector>

#define ENUMERATE_BYTECODE_INSTRUCTIONS(Op)         \
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
}

namespace quart::bytecode {

class BasicBlock;
class Generator;

class Instruction : public Value {
public:
    NO_COPY(Instruction)
    NO_MOVE(Instruction)

    static bool classof(Instruction const*) { return true; }

    virtual ~Instruction() = default;

    enum InstructionKind : u8 {
    #define Op(x) x, // NOLINT
        ENUMERATE_BYTECODE_INSTRUCTIONS(Op)
    #undef Op
    };

    InstructionKind kind() const { return m_kind; }
    BasicBlock* parent() const { return m_parent; }
    Instruction* next() const { return m_next; }

    template<typename T> requires(std::is_base_of_v<Instruction, T>)
    bool is() const {
        return T::classof(this);
    }

    template<typename T> requires(std::is_base_of_v<Instruction, T>)
    [[nodiscard]] T const* as() const {
        return T::classof(this) ? static_cast<T const*>(this) : nullptr;
    }

    virtual bool is_terminator() const { return false; }

    StringView kind_name() const {
        switch (m_kind) {
        #define Op(x) case x: return #x; // NOLINT
            ENUMERATE_BYTECODE_INSTRUCTIONS(Op)
        #undef Op
        }

        return {};
    }

    void set_parent(BasicBlock* parent) { m_parent = parent; }
    void set_next(Instruction* next) { m_next = next; }

    virtual void print(std::ostream&) const = 0;

protected:
    Instruction(Type* type, InstructionKind kind) : Value(type, InstructionID), m_kind(kind) {}

private:
    InstructionKind m_kind;
    BasicBlock* m_parent = nullptr;

    Instruction* m_next = nullptr;
};

template<Instruction::InstructionKind Ty>
class InstructionBase : public Instruction {
public:
    static bool classof(const Value* value) { return value->id() == InstructionID; }
    static bool classof(const Instruction* inst) { return inst->kind() == Ty; }

protected:
    InstructionBase(Type* type) : Instruction(type, Ty) {}
};

class NewArray : public InstructionBase<Instruction::NewArray> {
public:
    NewArray(Vector<Value*> elements, ArrayType* type);

    Vector<Value*> const& elements() const { return m_elements; }

    void print(std::ostream&) const override;

private:
    Vector<Value*> m_elements;
};

class GetMember : public InstructionBase<Instruction::GetMember> {
public:
    GetMember(Type* type, Value* src, Value* index);

    Value* src() const { return m_src; }
    Value* index() const { return m_index; }

    void print(std::ostream&) const override;

private:
    Value* m_src;
    Value* m_index;
};

class SetMember : public InstructionBase<Instruction::SetMember> {
public:
    SetMember(Value* dst, Value* index, Value* src);

    Value* dst() const { return m_dst; }
    Value* index() const { return m_index; }
    Value* src() const { return m_src; }

    void print(std::ostream&) const override;

private:
    Value* m_dst;
    Value* m_index;
    Value* m_src;
};

class GetMemberRef : public InstructionBase<Instruction::GetMemberRef> {
public:
    GetMemberRef(Type* type, Value* src, Value* index);

    Value* src() const { return m_src; }
    Value* index() const { return m_index; }

    void print(std::ostream&) const override;

private:
    Value* m_src;
    Value* m_index;
};

class NewLocalScope : public InstructionBase<Instruction::NewLocalScope> {
public:
    NewLocalScope(Function* function, bool set = true);

    Function* function() const { return m_function; }
    bool set() const { return m_set; }

    void print(std::ostream&) const override;
private:
    Function* m_function;
    bool m_set;
};

class GetLocal : public InstructionBase<Instruction::GetLocal> {
public:
    GetLocal(Type* type, u32 index) : InstructionBase(type), m_index(index) {}

    u32 index() const { return m_index; }

    void print(std::ostream&) const override;
private:
    u32 m_index;
};

class GetLocalRef : public InstructionBase<Instruction::GetLocalRef> {
public:
    GetLocalRef(Type* type, u32 index) : InstructionBase(type), m_index(index) {}

    u32 index() const { return m_index; }

    void print(std::ostream&) const override;

private:
    u32 m_index;
};

class SetLocal : public InstructionBase<Instruction::SetLocal> {
public:
    SetLocal(u32 index, Value* src);

    u32 index() const { return m_index; }
    Value* src() const { return m_src; }

    void print(std::ostream&) const override;

private:
    u32 m_index;
    Value* m_src;
};

class GetGlobal : public InstructionBase<Instruction::GetGlobal> {
public:
    GetGlobal(Type* type, u32 index) : InstructionBase(type), m_index(index) {}

    u32 index() const { return m_index; }

    void print(std::ostream&) const override;

private:
    u32 m_index;
};

class GetGlobalRef : public InstructionBase<Instruction::GetGlobalRef> {
public:
    GetGlobalRef(Type* type, u32 index) : InstructionBase(type), m_index(index) {}

    u32 index() const { return m_index; }

    void print(std::ostream&) const override;

private:
    u32 m_index;
};

class SetGlobal : public InstructionBase<Instruction::SetGlobal> {
public:
    SetGlobal(u32 index, Constant* src);

    u32 index() const { return m_index; }
    Constant* src() const { return m_src; }

    void print(std::ostream&) const override;

private:
    u32 m_index;
    Constant* m_src;
};

class Read : public InstructionBase<Instruction::Read> {
public:
    Read(Value* src);

    Value* src() const { return m_src; }

    void print(std::ostream&) const override;

private:
    Value* m_src;
};

class Write : public InstructionBase<Instruction::Write> {
public:
    Write(Value* dst, Value* src);

    Value* dst() const { return m_dst; }
    Value* src() const { return m_src; }

    void print(std::ostream&) const override;

private:
    Value* m_dst;
    Value* m_src;
};


// NOLINTNEXTLINE
#define DEFINE_ARITHMETIC_INSTRUCTION(name)                                                                             \
    class name : public InstructionBase<Instruction::name> { /* NOLINT */                                               \
    public:                                                                                                             \
        name(Value* lhs, Value* rhs) : InstructionBase(lhs->type()), m_lhs(lhs), m_rhs(rhs) {                           \
            m_lhs->add_user(this);                                                                                      \
            m_rhs->add_user(this);                                                                                      \
        }                                                                                                               \
                                                                                                                        \
        Value* lhs() const { return m_lhs; }                                                                            \
        Value* rhs() const { return m_rhs; }                                                                            \
                                                                                                                        \
        void print(std::ostream&) const override;                                                                       \
                                                                                                                        \
    private:                                                                                                            \
        Value* m_lhs;                                                                                                   \
        Value* m_rhs;                                                                                                   \
    };

ENUMERATE_BINARY_OPS(DEFINE_ARITHMETIC_INSTRUCTION)

#undef DEFINE_ARITHMETIC_INSTRUCTION

// `goto target`
class Jump : public InstructionBase<Instruction::Jump> {
public:
    Jump(BasicBlock* target);

    BasicBlock* target() const { return m_target; }

    bool is_terminator() const override { return true; }
    void print(std::ostream&) const override;

private:
    BasicBlock* m_target;
};

// `if (condition) { goto true_target } else { goto false_target }`
class JumpIf : public InstructionBase<Instruction::JumpIf> {
public:
    JumpIf(Value* condition, BasicBlock* true_target, BasicBlock* false_target);

    Value* condition() const { return m_condition; }

    BasicBlock* true_target() const { return m_true_target; }
    BasicBlock* false_target() const { return m_false_target; }

    bool is_terminator() const override { return true; }
    void print(std::ostream&) const override;

private:
    Value* m_condition;
    
    BasicBlock* m_true_target;
    BasicBlock* m_false_target;
};

class NewFunction : public InstructionBase<Instruction::NewFunction> {
public:
    NewFunction(Function* function);

    Function* function() const { return m_function; }

    void print(std::ostream& stream) const override;

private:
    Function* m_function;
};

class Return : public InstructionBase<Instruction::Return> {
public:
    Return(Value* value = nullptr);

    Value* value() const { return m_value; }

    bool is_terminator() const override { return true; }
    void print(std::ostream&) const override;

private:
    Value* m_value;
};

class Call : public InstructionBase<Instruction::Call> {
public:
    Call(
        Value* function, FunctionType const* function_type, Vector<Value*> arguments
    ) : InstructionBase(function_type->return_type()), m_function(function), m_function_type(function_type), m_arguments(move(arguments)) {
        function->add_user(this);
        for (auto& value : m_arguments) {
            value->add_user(this);
        }
    }

    Value* function() const { return m_function; }
    FunctionType const* function_type() const { return m_function_type; }
    Vector<Value*> const& arguments() const { return m_arguments; }

    void print(std::ostream&) const override;

private:
    Value* m_function;
    FunctionType const* m_function_type;
    Vector<Value*> m_arguments;
};

class Cast : public InstructionBase<Instruction::Cast> {
public:
    Cast(Value* src, Type* type) : InstructionBase(type), m_src(src) {
        m_src->add_user(this);
    }

    Value* src() const { return m_src; }

    void print(std::ostream&) const override;

private:
    Value* m_src;
};

class NewStruct : public InstructionBase<Instruction::NewStruct> {
public:
    NewStruct(Struct* structure);

    Struct* structure() const { return m_structure; }

    void print(std::ostream&) const override;

private:
    Struct* m_structure;
};

class Construct : public InstructionBase<Instruction::Construct> {
public:
    Construct(Struct* structure, Vector<Value*> arguments);

    Struct* structure() const { return m_structure; }
    Vector<Value*> const& arguments() const { return m_arguments; }

    void print(std::ostream&) const override;

private:
    Struct* m_structure;
    Vector<Value*> m_arguments;
};

class Alloca : public InstructionBase<Instruction::Alloca> {
public:
    Alloca(Type* type) : InstructionBase(type->get_pointer_to()) {}

    Type* get_allocated_type() const { return type()->get_pointee_type(); }

    void print(std::ostream&) const override;
};

class NewTuple : public InstructionBase<Instruction::NewTuple> {
public:
    NewTuple(TupleType* type, Vector<Value*> elements) : InstructionBase(type), m_elements(move(elements)) {
        for (auto& value : m_elements) {
            value->add_user(this);
        }
    }

    Vector<Value*> const& elements() const { return m_elements; }

    void print(std::ostream&) const override;

private:
    Vector<Value*> m_elements;
};

class Null : public InstructionBase<Instruction::Null> {
public:
    Null(Type* type) : InstructionBase(type) {}

    void print(std::ostream&) const override;
};

class Boolean : public InstructionBase<Instruction::Boolean> {
public:
    Boolean(bool value);

    bool value() const { return m_value; }

    void print(std::ostream&) const override;

private:
    bool m_value;
};

class Not : public InstructionBase<Instruction::Not> {
public:
    Not(Value* src) : InstructionBase(src->type()), m_src(src) {
        m_src->add_user(this);
    }

    Value* src() const { return m_src; }

    void print(std::ostream&) const override;

private:
    Value* m_src;
};

class Memcpy : public InstructionBase<Instruction::Memcpy> {
public:
    Memcpy(Value* dst, Value* src, size_t size) : InstructionBase(dst->type()), m_dst(dst), m_src(src), m_size(size) {
        m_src->add_user(this);
        m_dst->add_user(this);
    }

    Value* dst() const { return m_dst; }
    Value* src() const { return m_src; }
    size_t size() const { return m_size; }

    void print(std::ostream&) const override;

private:
    Value* m_dst;
    Value* m_src;
    size_t m_size;
};

class GetReturn : public InstructionBase<Instruction::GetReturn> {
public:
    GetReturn(Type* type) : InstructionBase(type) {}

    void print(std::ostream&) const override;
};

}