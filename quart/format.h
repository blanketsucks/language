#pragma once

#include <quart/common.h>

#include <llvm/Support/FormatVariadic.h>

namespace quart {

template<typename T>
using has_format_provider = llvm::support::detail::has_FormatProvider<T>;

template<typename T>
static constexpr bool has_format_provider_v = has_format_provider<T>::value;

template<typename ...Args>
using formatable = std::conjunction<has_format_provider<Args>...>;

template<typename ...Args>
static constexpr bool formatable_v = formatable<Args...>::value;

template<typename ...Args> requires(formatable_v<Args...>)
String format(const char* fmt, Args... args) {
    return llvm::formatv(fmt, std::forward<Args>(args)...).str();
}

template<typename ...Args> requires(formatable_v<Args...>)
void out(const char* fmt, Args... args) {
    auto format = llvm::formatv(fmt, std::forward<Args>(args)...);
    format.format(llvm::outs());
}

template<typename ...Args> requires(formatable_v<Args...>)
void outln(const char* fmt, Args... args) {
    auto format = llvm::formatv(fmt, std::forward<Args>(args)...);
    format.format(llvm::outs());
    
    llvm::outs() << '\n';
}

void outln(const char*);
void outln(const String&);
void outln();

template<typename ...Args> requires(formatable_v<Args...>)
void errln(const char* fmt, Args... args) {
    auto format = llvm::formatv(fmt, std::forward<Args>(args)...);
    format.format(llvm::errs());
    
    llvm::errs() << '\n';
}

}