#ifndef _UTILS_POINTER_H
#define _UTILS_POINTER_H

#include <memory>

namespace utils {

template<typename T> using Ref = std::unique_ptr<T>;
template<typename T, typename ...Args> Ref<T> make_ref(Args&& ...args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T> using Shared = std::shared_ptr<T>;
template<typename T, typename ...Args> Shared<T> make_shared(Args&& ...args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

}

#endif