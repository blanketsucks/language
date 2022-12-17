#ifndef _UTILS_H
#define _UTILS_H

#include <assert.h>
#include <vector>
#include <utility>

namespace utils {

template<typename F, typename S> 
std::vector<std::pair<F, S>> zip(std::vector<F> first, std::vector<S> second) {
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
