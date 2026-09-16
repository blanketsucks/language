#include <quart/bytecode/instruction.h>
#include <quart/bytecode/basic_block.h>
#include <quart/language/functions.h>
#include <quart/language/structs.h>
#include <quart/language/state.h>

namespace quart::bytecode {

static inline String fmt(Constant* constant) {
    std::stringstream stream;
    constant->print(stream);

    return stream.str();
}

static inline String fmt(Value* value) {
    switch (value->id()) {
        case Value::InstructionID: {
            return format("%{}", value->value_name());
        }
        case Value::FunctionID: {
            auto* function = cast_unchecked<Function>(value);
            return function->qualified_name();
        }
        case Value::ConstantID: {
            return fmt(cast_unchecked<Constant>(value));
        }
        case Value::Unknown: {
            return "???";
        }
    }
}

static inline String fmt(const Vector<Value*>& values) {
    return format_range(values, [](Value* v) { return fmt(v); });
}

static inline void print_prefix(const Instruction* instruction, std::ostream& stream) {
    stream << '%' << instruction->value_name() << " = ";
}

NewArray::NewArray(Vector<Value*> elements, ArrayType* type) : InstructionBase(type), m_elements(move(elements)) {
    for (auto& value : m_elements) {
        value->add_user(this);
    }
}

void NewArray::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << std::format("NewArray {}", fmt(m_elements));
}

NewLocalScope::NewLocalScope(Function* function, bool set) : InstructionBase(function->underlying_type()), m_function(function), m_set(set) {}

void NewLocalScope::print(std::ostream& stream) const {
    stream << "NewLocalScope";
}

void GetLocal::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << std::format("GetLocal {}", m_index);
}

void GetLocalRef::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << std::format("GetLocalRef {}", m_index);
}

SetLocal::SetLocal(u32 index, Value* src) : InstructionBase(src->type()), m_index(index), m_src(src) {
    src->add_user(this);
}

void SetLocal::print(std::ostream& stream) const {
    print_prefix(this, stream);
    if (m_src) {
        stream << format("SetLocal {}, {}", m_index, fmt(m_src));
    } else {
        stream << format("SetLocal {}, {{}}", m_index);
    }
}

void GetGlobal::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << std::format("GetGlobal {}", m_index);
}

void GetGlobalRef::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << std::format("GetGlobalRef {}", m_index);
}

SetGlobal::SetGlobal(u32 index, Constant* src) : InstructionBase(src->type()), m_index(index), m_src(src) {
    src->add_user(this);
}

void SetGlobal::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << format("SetGlobal {}, {}", m_index, fmt(m_src));
}

GetMember::GetMember(Type* type, Value* src, Value* index) : InstructionBase(type), m_src(src), m_index(index) {
    m_src->add_user(this);
    m_index->add_user(this);
}

void GetMember::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << std::format("GetMember {}, {}", fmt(m_src), fmt(m_index));
}

GetMemberRef::GetMemberRef(Type* type, Value* src, Value* index) : InstructionBase(type), m_src(src), m_index(index) {
    m_src->add_user(this);
    m_index->add_user(this);
}

void GetMemberRef::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << std::format("GetMemberRef {}, {}", fmt(m_src), fmt(m_index));
}

SetMember::SetMember(Value* dst, Value* index, Value* src) : InstructionBase(src->type()), m_dst(dst), m_index(index), m_src(src) {
    dst->add_user(this);
    index->add_user(this);
    src->add_user(this);
}

void SetMember::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << format("SetMember {}, {}, {}", fmt(m_src), fmt(m_index), fmt(m_dst));
}

Read::Read(Value* src) : InstructionBase(src->type()->underlying_type()), m_src(src) {
    src->add_user(this);
}

void Read::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << format("Read {}", fmt(m_src));
}

Write::Write(Value* dst, Value* src) : InstructionBase(src->type()), m_dst(dst), m_src(src) {
    dst->add_user(this);
    src->add_user(this);
}

void Write::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << format("Write {}, {}", fmt(m_dst), fmt(m_src));
}

// NOLINTNEXTLINE
#define Op(Inst)                                                            \
    void Inst::print(std::ostream& stream) const {                          \
        print_prefix(this, stream);                                         \
        outln(#Inst " {}, {}", fmt(m_lhs), fmt(m_rhs));                     \
    }                                                                       \

ENUMERATE_BINARY_OPS(Op)

#undef Op

Jump::Jump(BasicBlock* block) : InstructionBase(nullptr), m_target(block) {}

void Jump::print(std::ostream& stream) const {
    stream << format("Jump {}", m_target->name());
}

JumpIf::JumpIf(
    Value* condition, BasicBlock* true_target, BasicBlock* false_target
) : InstructionBase(nullptr), m_condition(condition), m_true_target(true_target), m_false_target(false_target) {
    condition->add_user(this);
}

void JumpIf::print(std::ostream& stream) const {
    stream << format("JumpIf {}, {}, {}", fmt(m_condition), m_true_target->name(), m_false_target->name());
}

NewFunction::NewFunction(Function* function) : InstructionBase(nullptr), m_function(function) {} 

void NewFunction::print(std::ostream& stream) const {
    stream << format("NewFunction {}", m_function->qualified_name());
}

Return::Return(Value* value) : InstructionBase(nullptr), m_value(value) {
    if (value) {
        value->add_user(this);
    }
}

void Return::print(std::ostream& stream) const {
    if (m_value) {
        stream << format("Return {}", fmt(m_value));
    } else {
        stream << "Return";
    }
}

void Call::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << format("Call {}, {}", fmt(m_function), fmt(m_arguments));
}

void Cast::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << format("Cast {}, {}", fmt(m_src), type()->str());
}

NewStruct::NewStruct(Struct* structure) : InstructionBase(nullptr), m_structure(structure) {}

void NewStruct::print(std::ostream& stream) const {
    stream << format("NewStruct {}", m_structure->qualified_name());
}

Construct::Construct(
    Struct* structure, Vector<Value*> arguments
) : InstructionBase(structure->underlying_type()), m_structure(structure), m_arguments(move(arguments)) {
    for (auto* value : m_arguments) {
        value->add_user(this);
    }
} 

void Construct::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << format("Construct {}, {}", m_structure->qualified_name(), fmt(m_arguments));
}

void Alloca::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << format("Alloca {}", get_allocated_type()->str());
}

void Null::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << format("Null {}", type()->str());
}

void Boolean::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << format("Boolean {}", static_cast<i32>(m_value));
}

void Not::print(std::ostream& stream) const {
    print_prefix(this, stream);
    stream << format("Not {}", fmt(m_src));
}

void Memcpy::print(std::ostream& stream) const {
    stream << format("Memcpy {}, {}, {}", fmt(m_dst), fmt(m_src), m_size);
}

void GetReturn::print(std::ostream& stream) const {
    stream << "GetReturn";
}

void NewTuple::print(std::ostream& stream) const {
    stream << format("NewTuple {}", fmt(m_elements));
}

}