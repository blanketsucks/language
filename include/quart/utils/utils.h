#ifndef _UTILS_UTILS_H
#define _UTILS_UTILS_H

#include <assert.h>
#include <vector>
#include <utility>
#include <algorithm>

#undef alloca

#ifdef __GNUC__
    #define __UNREACHABLE __builtin_unreachable();
#elif _MSC_VER
    #define __UNREACHABLE __assume(false);
#else
    #define __UNREACHABLE
#endif

namespace utils {

template<typename F, typename S> 
std::vector<std::pair<F, S>> zip(
    const std::vector<F>& first, const std::vector<S>& second
) {
    assert(first.size() == second.size() && "first and second must have the same size");

    std::vector<std::pair<F, S>> result;
    result.reserve(first.size());

    for (size_t i = 0; i < first.size(); i++) {
        result.push_back(std::make_pair(first[i], second[i]));
    }

    return result;
}

template<typename T> bool contains(const std::vector<T>& vec, const T& value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

}

#endif
