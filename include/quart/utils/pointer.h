#pragma once

#include <memory>

namespace utils {

template<typename T> using Scope = std::unique_ptr<T>;
template<typename T, typename ...Args> Scope<T> make_scope(Args&& ...args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T> using Ref = std::shared_ptr<T>;
template<typename T, typename ...Args> Ref<T> make_ref(Args&& ...args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

}
