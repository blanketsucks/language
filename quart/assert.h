#pragma once

#include <quart/lexer/lexer.h>

#define ASSERT(x, message)                                                                  \
    do {                                                                                    \
        if (!(x)) {                                                                         \
            quart::assertion_failed(message, #x, __FILE__, __LINE__, __PRETTY_FUNCTION__);  \
            LLVM_BUILTIN_TRAP;                                                              \
        }                                                                                   \
    } while (0)                                                                             \

namespace quart {

enum class Color : u8 {
    Reset = 0,
    Red = 31,
    White = 37,
    Magenta = 35
};

bool has_color_support();
void assertion_failed(StringView message, StringView condition, StringView file, u32 line, StringView function);

}
