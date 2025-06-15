#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <map>
#include <optional>
#include <string_view>
#include <string>
#include <deque>
#include <set>

#ifndef QUART_PATH
    #define QUART_PATH "lib"
#endif

#define FILE_EXTENSION ".qr"

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_NAME "windows"
#elif defined(__APPLE__)
    #define PLATFORM_NAME "macos"
#elif defined(__ANDROID__)
    #define PLATFORM_NAME "android"
#elif defined(__linux__)
    #define PLATFORM_NAME "linux"
#else
    #define PLATFORM_NAME "unknown"
#endif

#ifdef __GNUC__
    #define UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
    #define UNREACHABLE() __assume(0)
#else
    #define UNREACHABLE() assert(false && "Unreachable")
#endif

#define NO_COPY(Class)                                  \
    Class(const Class&) = delete;                       \
    Class& operator=(const Class&) = delete;

#define NO_MOVE(Class)                                  \
    Class(Class&&) = delete;                            \
    Class& operator=(Class&&) = delete;

#define DEFAULT_COPY(Class)                             \
    Class(const Class&) = default;                      \
    Class& operator=(const Class&) = default;

#define DEFAULT_MOVE(Class)                             \
    Class(Class&&) = default;                           \
    Class& operator=(Class&&) = default;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

namespace quart {

using String = std::string;
using StringView = std::string_view;

template<typename T, typename ...Args>
using of_type = std::conjunction<std::is_same<Args, T>...>;

template<typename T, typename ...Args>
inline constexpr bool of_type_v = of_type<T, Args...>::value;

template<typename K, typename V> using HashMap = std::map<K, V>;

template<typename T> using Vector = std::vector<T>;
template<typename T> using Deque = std::deque<T>;

template<typename T, size_t N> using Array = std::array<T, N>;

template<typename T> using Set = std::set<T>;

template<typename T> using Optional = std::optional<T>;

template<typename F, typename S> using Pair = std::pair<F, S>;

template<typename T> using RefPtr = std::shared_ptr<T>;
template<typename T> using OwnPtr = std::unique_ptr<T>;

using std::move;

template<typename T, typename ...Args>
inline RefPtr<T> make_ref(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template<typename T, typename ...Args>
inline OwnPtr<T> make(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

enum class LinkageSpecifier : u8 {
    None,
    Unspecified,
    C,
};

}
