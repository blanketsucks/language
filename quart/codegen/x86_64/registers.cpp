#include <quart/codegen/x86_64/registers.h>

namespace quart::x86_64 {

StringView Register::as_qword() const {
    switch (type) {
        case rax: return "rax";
        case rbx: return "rbx";
        case rcx: return "rcx";
        case rdx: return "rdx";
        case rsi: return "rsi";
        case rdi: return "rdi";
        case r8:  return "r8";
        case r9:  return "r9";
        case r10: return "r10";
        case r11: return "r11";
        case r12: return "r12";
        case r13: return "r13";
        case r14: return "r14";
        case r15: return "r15";
        default:
            return "???";
    }
}

StringView Register::as_dword() const {
    switch (type) {
        case rax: return "eax";
        case rbx: return "ebx";
        case rcx: return "ecx";
        case rdx: return "edx";
        case rsi: return "esi";
        case rdi: return "edi";
        case r8:  return "r8d";
        case r9:  return "r9d";
        case r10: return "r10d";
        case r11: return "r11d";
        case r12: return "r12d";
        case r13: return "r13d";
        case r14: return "r14d";
        case r15: return "r15d";
        default:
            return "???";
    }
}

StringView Register::as_word() const {
    switch (type) {
        case rax: return "ax";
        case rbx: return "bx";
        case rcx: return "cx";
        case rdx: return "dx";
        case rsi: return "si";
        case rdi: return "di";
        case r8:  return "r8w";
        case r9:  return "r9w";
        case r10: return "r10w";
        case r11: return "r11w";
        case r12: return "r12w";
        case r13: return "r13w";
        case r14: return "r14w";
        case r15: return "r15w";
        default:
            return "???";
    }
}

StringView Register::as_byte() const {
    switch (type) {
        case rax: return "al";
        case rbx: return "bl";
        case rcx: return "cl";
        case rdx: return "dl";
        case rsi: return "sil";
        case rdi: return "dil";
        case r8:  return "r8b";
        case r9:  return "r9b";
        case r10: return "r10b";
        case r11: return "r11b";
        case r12: return "r12b";
        case r13: return "r13b";
        case r14: return "r14b";
        case r15: return "r15b";
        default:
            return "???";
    }
}

StringView Register::as(DataType data_type) const {
    switch (data_type) {
        case DataType::Byte: return as_byte();
        case DataType::Word: return as_word();
        case DataType::DWord: return as_dword();
        case DataType::QWord: return as_qword();
    }

    return "???";
}

}