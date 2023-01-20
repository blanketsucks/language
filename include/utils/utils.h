#ifndef _UTILS_H
#define _UTILS_H

#include <assert.h>
#include <vector>
#include <utility>

#ifdef __GNUC__
    #define __UNREACHABLE __builtin_unreachable();
#elif _MSC_VER
    #undef alloca
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

}

#endif
