#pragma once

#include <quart/common.h>

#include <format>   
#include <iostream>

namespace quart {

using std::format;

template<typename ...Args>
String dyn_format(StringView fmt, Args&&... args) {
    return std::vformat(fmt, std::make_format_args(args...));
}

template<typename ...Args>
void out(std::format_string<Args...> fmt, Args... args) {
    String str = std::format(fmt, std::forward<Args>(args)...);
    std::cout << str;
}

template<typename ...Args>
void outln(std::format_string<Args...> fmt, Args... args) {
    String str = std::format(fmt, std::forward<Args>(args)...);
    std::cout << str << '\n';
}

void outln(const char*);
void outln(const String&);
void outln();

template<typename ...Args>
void errln(std::format_string<Args...> fmt, Args... args) {
    String str = std::format(fmt, std::forward<Args>(args)...);
    std::cerr << str << '\n';
}

}