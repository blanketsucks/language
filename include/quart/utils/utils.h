#pragma once

#include <assert.h>
#include <vector>
#include <utility>
#include <algorithm>

#undef alloca

namespace utils {

template<typename T> bool contains(const std::vector<T>& vec, const T& value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

}
