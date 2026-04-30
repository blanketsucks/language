#pragma once

#include <quart/common.h>

#include <format>

namespace quart::x86_64 {

enum class ConditionCode {
    None,
    a,      // Above
    ae,     // Above or equal
    b,      // Below
    be,     // Below or equal
    c,      // Carry
    g,      // Greater
    ge,     // Greater or equal
    l,      // Less
    le,     // Less or equal
    na,     // Not above
    nae,    // Not above or equal
    nb,     // Not below
    nbe,    // Not below or equal
    nc,     // Not carry
    ng,     // Not greater
    nge,    // Not greater or equal
    nl,     // Not less
    nle,    // Not less or equal
    no,     // Not overflow
    np,     // Not parity
    nz,     // Not zero
    o,      // Overflow
    p,      // Parity
    pe,     // Parity even
    po,     // Parity odd
    s,      // Sign
    z,      // Zero,
    e,      // Equal
    ne,     // Not equal
    ns,     // Not sign
};

enum class BinaryInstruction {
    cmp,
    add,
    sub,
    mul,
    sal,
    sar,
    shr,
    shl,
};

enum class DataType {
    Byte  = 1,
    Word  = 2,
    DWord = 4,
    QWord = 8
};

StringView to_string(ConditionCode cc);
StringView to_string(BinaryInstruction instruction);
StringView to_string(DataType data_type);

ConditionCode negate(ConditionCode cc);

}

// NOLINTNEXTLINE
#define DEFINE_ENUM_FORMATTER(cls)                                                                                  \                
    template <>                                                                                                     \
    struct std::formatter<cls> {                                                                                    \
        constexpr auto parse(std::format_parse_context& ctx) {                                                      \
            return ctx.begin();                                                                                     \
        }                                                                                                           \
                                                                                                                    \
        auto format(const cls& cc, std::format_context& ctx) const {                                                \
            return std::format_to(ctx.out(), "{}", quart::x86_64::to_string(cc));                                   \
        }                                                                                                           \
    };

DEFINE_ENUM_FORMATTER(quart::x86_64::ConditionCode)
DEFINE_ENUM_FORMATTER(quart::x86_64::BinaryInstruction)
DEFINE_ENUM_FORMATTER(quart::x86_64::DataType)

#undef DEFINE_ENUM_FORMATTER