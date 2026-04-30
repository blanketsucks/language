#pragma once

#include <quart/common.h>
#include <quart/codegen/x86_64/cpu.h>

namespace quart::x86_64 {

struct Register {
    enum Type {
        None = 0,
        rax,
        rbx,
        rcx,
        rdx,
        rsi,
        rdi,
        r8,
        r9,
        r10,
        r11,
        r12,
        r13,
        r14,
        r15
    };

    Type type;

    StringView as(DataType) const;

    StringView as_qword() const;
    StringView as_dword() const;
    StringView as_word()  const;
    StringView as_byte()  const;
};

}