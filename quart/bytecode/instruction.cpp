#include <quart/bytecode/instruction.h>
#include <quart/bytecode/basic_block.h>

namespace quart::bytecode {

static inline String format_operand(Register reg) {
    return format("r{0}", reg.index());
}

static inline String format_operand(const Operand& operand) {
    switch (operand.type()) {
        case Operand::None:
            return "{ empty value }";
        case Operand::Register:
            return format("r{0}", operand.value());
        case Operand::Immediate:
            return format("{0}", operand.value());
    }

    return "{ unknown operand type }";
}

void Move::dump() const {
    outln("Move { dst: {0}, src: {1} }", format_operand(m_dst), format_operand(m_src));
}

void NewString::dump() const {
    outln("NewString { dst: {0}, value: {1} }", format_operand(m_dst), m_value);
}

void NewArray::dump() const {
    outln("NewArray { dst: {0}, size: {1} }", format_operand(m_dst), m_elements.size());
}

void NewLocalScope::dump() const {
    outln("NewLocalScope { function: {0} }", m_function);
}

void GetLocal::dump() const {
    outln("GetLocal { dst: {0}, index: {1} }", format_operand(m_dst), m_index);
}

void GetLocalRef::dump() const {
    outln("GetLocalRef { dst: {0}, index: {1} }", format_operand(m_dst), m_index);
}

void SetLocal::dump() const {
    outln("SetLocal { src: {0}, index: {1} }", format_operand(m_src), m_index);
}

void GetGlobal::dump() const {
    outln("GetGlobal { dst: {0}, index: {1} }", format_operand(m_dst), m_index);
}

void GetGlobalRef::dump() const {
    outln("GetGlobalRef { dst: {0}, index: {1} }", format_operand(m_dst), m_index);
}

void SetGlobal::dump() const {
    outln("SetGlobal { src: {0}, index: {1} }", format_operand(m_src), m_index);
}

void GetMember::dump() const {
    outln("GetMember { dst: {0}, src: {1}, index: {2} }", format_operand(m_dst), format_operand(m_src), m_index);
}

void GetMemberRef::dump() const {
    outln("GetMemberRef { dst: {0}, src: {1}, index: {2} }", format_operand(m_dst), format_operand(m_src), m_index);
}

void SetMember::dump() const {
    outln("SetMember { src: {0}, dst: {1}, index: {2} }", format_operand(m_src), format_operand(m_dst), m_index);
}

void Read::dump() const {
    outln("Read { dst: {0}, src: {1} }", format_operand(m_dst), format_operand(m_src));
}

void Write::dump() const {
    outln("Write { dst: {0}, src: {1} }", format_operand(m_dst), format_operand(m_src));
}

// NOLINTNEXTLINE
#define Op(x)                                                                                                                   \
    void x::dump() const {                                                                                                      \
        outln("{0} { dst: {1}, lhs: {2}, rhs: {3} }", #x, format_operand(m_dst), format_operand(m_lhs), format_operand(m_rhs)); \
    }

ENUMERATE_BINARY_OPS(Op)

#undef Op

void Jump::dump() const {
    outln("Jump { target: {0} }", m_target->name());
}

void JumpIf::dump() const {
    outln("JumpIf { condition: {0}, true_target: {1}, false_target: {2} }", format_operand(m_condition), m_true_target->name(), m_false_target->name());
}

void NewFunction::dump() const {
    outln("NewFunction { function: {0} }", m_function);
}

void GetFunction::dump() const {
    outln("GetFunction { dst: {0}, function: {1} }", format_operand(m_dst), m_function);
}

void Return::dump() const {
    if (m_value.has_value()) {
        outln("Return { value: {0} }", format_operand(m_value.value()));
    } else {
        outln("Return { value: None }");
    }
}

void Call::dump() const {
    outln("Call { dst: {0}, function: {1}, args: {2} }", format_operand(m_dst), format_operand(m_function), m_arguments.size());
}

void Cast::dump() const {
    outln("Cast { dst: {0}, src: {1}, type: {2} }", format_operand(m_dst), format_operand(m_src), m_type->str());
}

void NewStruct::dump() const {
    outln("NewStruct { struct: {0} }", m_structure);
}

void Construct::dump() const {
    outln("Construct { dst: {0}, struct: {1}, args: {2} }", format_operand(m_dst), m_structure, m_arguments.size());
}

}