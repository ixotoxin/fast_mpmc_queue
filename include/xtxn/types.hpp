// Copyright (c) 2026 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <type_traits>
#include <new>
#include <atomic>

namespace xtxn {
    template<class T>
    concept any_atomic_int
        = requires { typename T::value_type; }
          && std::is_integral_v<typename T::value_type>
          && requires(T t) { [] <class U> (std::atomic<U> &) {} (t); };

    template<class T>
    concept any_atomic_uint
        = requires { typename T::value_type; }
          && std::is_integral_v<typename T::value_type>
          && std::is_unsigned_v<typename T::value_type>
          && requires(T t) { [] <class U> (std::atomic<U> &) {} (t); };

#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t hw_cis { std::hardware_constructive_interference_size };
    constexpr std::size_t hw_dis { std::hardware_destructive_interference_size };
#else
    constexpr std::size_t hw_cis { 64 };
    constexpr std::size_t hw_dis { 64 };
#endif
}
