#pragma once

#include <utility>
#include <type_traits>

#define MAKE_ENUM_BITWISE_OPS(Enum)                                                     \
    constexpr Enum operator|(Enum lhs, Enum rhs) {                                      \
        return static_cast<Enum>(std::to_underlying(lhs) | std::to_underlying(rhs));    \
    }                                                                                   \
                                                                                        \
    constexpr Enum operator&(Enum lhs, Enum rhs) {                                      \
        return static_cast<Enum>(std::to_underlying(lhs) & std::to_underlying(rhs));    \
    }                                                                                   \
                                                                                        \
    constexpr Enum operator^(Enum lhs, Enum rhs) {                                      \
        return static_cast<Enum>(std::to_underlying(lhs) ^ std::to_underlying(rhs));    \
    }                                                                                   \
                                                                                        \
    constexpr Enum operator~(Enum e) {                                                  \
        return static_cast<Enum>(~std::to_underlying(e));                               \
    }                                                                                   \
                                                                                        \
    constexpr Enum& operator|=(Enum& lhs, Enum rhs) {                                   \
        lhs = static_cast<Enum>(std::to_underlying(lhs) | std::to_underlying(rhs));     \
        return lhs;                                                                     \
    }                                                                                   \
                                                                                        \
    constexpr Enum& operator&=(Enum& lhs, Enum rhs) {                                   \
        lhs = static_cast<Enum>(std::to_underlying(lhs) & std::to_underlying(rhs));     \
        return lhs;                                                                     \
    }                                                                                   \
                                                                                        \
    constexpr Enum& operator^=(Enum& lhs, Enum rhs) {                                   \
        lhs = static_cast<Enum>(std::to_underlying(lhs) ^ std::to_underlying(rhs));     \
        return lhs;                                                                     \
    }          

namespace quart {

template<typename T> requires(std::is_enum_v<T>)
constexpr bool has_flag(T value, T mask) {
    return std::to_underlying(value & mask) == std::to_underlying(mask);
}

template<typename T> requires(std::is_integral_v<T>)
constexpr bool has_flag(T value, T mask) {
    return (value & mask) == mask;
}

template<typename T, typename U> requires(std::is_integral_v<T> && std::is_integral_v<U>)
constexpr bool has_flag(T value, U mask) {
    return (value & static_cast<T>(mask)) == static_cast<T>(mask);
}

template<typename T> requires(std::is_enum_v<T>)
constexpr bool has_any_flag(T value, T mask) {
    return to_underlying(value & mask) != 0;
}

template<typename T> requires(std::is_integral_v<T>)
constexpr bool has_any_flag(T value, T mask) {
    return (value & mask) != 0;
}

template<typename T, typename U> requires(std::is_integral_v<T> && std::is_integral_v<U>)
constexpr bool has_any_flag(T value, U mask) {
    return (value & static_cast<T>(mask)) != 0;
}

}