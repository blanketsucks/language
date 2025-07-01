#pragma once

#include <quart/common.h>

#include <format>   
#include <iostream>
#include <iterator>
#include <type_traits>

namespace quart {

template<typename Container>
using ContainerIterator = decltype(std::declval<Container>().begin());

using std::format;

template<typename ...Args>
String dyn_format(StringView fmt, Args&&... args) {
    return std::vformat(fmt, std::make_format_args(args...));
}

template<typename Container> requires std::is_same_v<typename std::iterator_traits<ContainerIterator<Container>>::value_type, String>
String format_range(const Container& container) {
    String result;
    if (container.empty()) {
        return result;
    }

    auto it = container.begin();
    
    result.append(*it);
    ++it;

    for (; it != container.end(); ++it) {
        result.append(", ");
        result.append(*it);
    }

    return result;
}

template<typename Container, typename F>
String format_range(const Container& container, F&& formatter) {
    String result;

    if (container.empty()) {
        return result;
    }

    auto it = container.begin();

    result.append(formatter(*it));
    ++it;

    for (; it != container.end(); ++it) {
        result.append(", ");
        result.append(formatter(*it));
    }

    return result;
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