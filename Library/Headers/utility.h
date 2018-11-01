#pragma once
#include <type_traits>

namespace task {
    template <class T, class U = void>
    struct type_exists : std::false_type { };

    template <class T>
    struct type_exists<T, std::void_t<T>> : std::true_type { };

    template <class T>
    using type_exists_t = typename type_exists<T>::type;

    template <class T>
    constexpr auto type_exists_v = type_exists<T>::value;
}