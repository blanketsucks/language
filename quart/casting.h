#pragma once

#include <type_traits>
#include <concepts>
#include <memory>

namespace quart {

namespace detail {

template<typename To, typename From>
struct isa_impl {
    static bool isa(const From& v) { return To::classof(const_cast<From*>(&v)); }
};

template<typename To, typename From> requires(std::is_base_of_v<To, From>)
struct isa_impl<To, From> {
    static bool isa(const From&) { return true; }
};

template<typename To, typename From>
struct isa_impl<To, From*> {
    static bool isa(From*& v) { return isa_impl<To, From>::isa(*v); }
};

template<typename To, typename From>
struct isa_impl<To, std::shared_ptr<From>> {
    static bool isa(const std::shared_ptr<From>& v) { return isa_impl<To, From>::isa(*v); }
};

template<typename To, typename From>
struct isa_impl<To, std::unique_ptr<From>> {
    static bool isa(const std::unique_ptr<From>& v) { return isa_impl<To, From>::isa(*v); }
};

template<typename To, typename From>
struct cast_impl {
    static To* cast(const From& v) { return (To*)&v; }
};

template<typename To, typename From>
struct cast_impl<To, From*> {
    static To* cast(From*& v) { return (To*)v; }
};

template<typename To, typename From>
struct cast_impl<To, std::shared_ptr<From>> {
    static To* cast(const std::shared_ptr<From>& v) { return cast_impl<To, From>::cast(*v); }
};

template<typename To, typename From>
struct cast_impl<To, std::unique_ptr<From>> {
    static To* cast(const std::unique_ptr<From>& v) { return cast_impl<To, From>::cast(*v); }
};

}

template<typename To, typename From>
inline bool isa(const From& value) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    return detail::isa_impl<To, From>::isa(const_cast<From&>(value));
}

template<typename To, typename From>
inline To* cast_unchecked(const From& value) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    return detail::cast_impl<To, From>::cast(const_cast<From&>(value));
}

template<typename To, typename From>
inline To* cast(const From& value) {
    if (!isa<To>(value)) {
        return nullptr;
    }

    return cast_unchecked<To>(value);
}

}