#include <quart/bytecode/instruction.h>
#include <quart/bytecode/basic_block.h>
#include <quart/language/functions.h>
#include <quart/language/structs.h>

namespace quart::bytecode {

String escape(const String& in) {
    String out;
    out.reserve(in.size()); // We reserve at least the size of the input string at first

    for (auto& c : in) {
        if (std::isprint(c)) {
            out.push_back(c);
            continue;
        }

        out.push_back('\\');
        switch (c) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '\n': out.push_back('n'); break;
            case '\r': out.push_back('r'); break;
            case '\t': out.push_back('t'); break;
            default:
                // FIXME: Handle hex case
                out.push_back(c);
        }
    }

    return out;
}

static inline String fmt(Register reg) {
    return format("r{0}", reg.index());
}

static inline String fmt(Operand const& operand) {
    if (operand.is_register()) {
        return format("r{0}", operand.value());
    } else {
        return format("{0}", operand.value());
    }
}

static String fmt(Vector<Register> registers) {
    String str = "[";
    for (auto [index, reg] : llvm::enumerate(registers)) {
        str.append(fmt(reg));
        if (index == registers.size() - 1) {
            continue;
        }

        str.append(", ");
    }

    str.push_back(']');
    return str;
}

void Move::dump() const {
    outln("Move {0}, {1}", fmt(m_dst), m_src);
}

void NewString::dump() const {
    outln("NewString {0}, \"{1}\"", fmt(m_dst), escape(m_value));
}

void NewArray::dump() const {
    outln("NewArray {0}, {1}", fmt(m_dst), fmt(m_elements));
}

void NewLocalScope::dump() const {
    outln("NewLocalScope");
}

void GetLocal::dump() const {
    outln("GetLocal {0}, {1}", fmt(m_dst), m_index);
}

void GetLocalRef::dump() const {
    outln("GetLocalRef {0}, {1}", fmt(m_dst), m_index);
}

void SetLocal::dump() const {
    if (m_src.has_value()) {
        outln("SetLocal {0}, {1}", m_index, fmt(*m_src));
    } else {
        outln("SetLocal {0}, {}", m_index);
    }
}

void GetGlobal::dump() const {
    outln("GetGlobal {0}, {1}", fmt(m_dst), m_index);
}

void GetGlobalRef::dump() const {
    outln("GetGlobalRef {0}, {1}", fmt(m_dst), m_index);
}

void SetGlobal::dump() const {
    outln("SetGlobal {0}, {1}", m_index, "{}");
}

void GetMember::dump() const {
    outln("GetMember {0}, {1}, {2}", fmt(m_dst), fmt(m_src), fmt(m_index));
}

void GetMemberRef::dump() const {
    outln("GetMemberRef {0}, {1}, {2}", fmt(m_dst), fmt(m_src), fmt(m_index));
}

void SetMember::dump() const {
    outln("SetMember {0}, {1}, {2}", fmt(m_src), fmt(m_dst), fmt(m_index));
}

void Read::dump() const {
    outln("Read {0}, {1}", fmt(m_dst), fmt(m_src));
}

void Write::dump() const {
    outln("Write {0}, {1}", fmt(m_dst), fmt(m_src));
}

// NOLINTNEXTLINE
#define Op(x)                                                                                                                   \
    void x::dump() const {                                                                                                      \
        outln("{0} {1}, {2}, {3}", #x, fmt(m_dst), fmt(m_lhs), fmt(m_rhs)); \
    }

ENUMERATE_BINARY_OPS(Op)

#undef Op

void Jump::dump() const {
    outln("Jump {0}", m_target->name());
}

void JumpIf::dump() const {
    outln("JumpIf {0}, {1}, {2}", fmt(m_condition), m_true_target->name(), m_false_target->name());
}

void NewFunction::dump() const {
    outln("NewFunction {0}", m_function->qualified_name());
}

void GetFunction::dump() const {
    outln("GetFunction {0}, {1}", fmt(m_dst), m_function->qualified_name());
}

void Return::dump() const {
    if (m_value.has_value()) {
        outln("Return {0}", fmt(m_value.value()));
    } else {
        outln("Return");
    }
}

void Call::dump() const {
    outln("Call {0}, {1}, {2}", fmt(m_dst), fmt(m_function), fmt(m_arguments));
}

void Cast::dump() const {
    outln("Cast {0}, {1}, {2}", fmt(m_dst), fmt(m_src), m_type->str());
}

void NewStruct::dump() const {
    outln("NewStruct {0}", m_structure->qualified_name());
}

void Construct::dump() const {
    outln("Construct {0}, {1}, {2}", fmt(m_dst), m_structure->qualified_name(), fmt(m_arguments));
}

void Alloca::dump() const {
    outln("Alloca {0}, {1}", fmt(m_dst), m_type->str());
}

void Null::dump() const {
    outln("Null {0}, {1}", fmt(m_dst), m_type->str());
}

void Boolean::dump() const {
    outln("Boolean {0}, {1}", fmt(m_dst), static_cast<i32>(m_value));
}

void Not::dump() const {
    outln("Not {0}, {1}", fmt(m_dst), fmt(m_src));
}

void Memcpy::dump() const {
    outln("Memcpy {0}, {1}, {2}", fmt(m_dst), fmt(m_src), m_size);
}

void GetReturn::dump() const {
    outln("GetReturn {0}", fmt(m_dst));
}

}