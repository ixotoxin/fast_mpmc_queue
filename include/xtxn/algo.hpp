// Copyright (c) 2026 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <type_traits>
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

    /** Post-increment iteration **/
    template<size_t B, any_atomic_uint T, typename U = T::value_type>
    requires (B > 1)
    U iterate_post_inc(T & value) noexcept {
        U current { value.fetch_add(1, std::memory_order_relaxed) };
        U next { current + 1 };
        if (next >= static_cast<U>(B)) {
    // v1 {{{
            // value.compare_exchange_weak(next, next % static_cast<U>(B), std::memory_order_relaxed);
    // }}} v1
    // v2 {{{
            // auto next2 = next - static_cast<U>(B);
            // while (next2 >= static_cast<U>(B)) next2 -= static_cast<U>(B);
            // value.compare_exchange_weak(next, next2, std::memory_order_relaxed);
    // }}} v2
    // v3 {{{
            value.compare_exchange_weak(next, next - static_cast<U>(B), std::memory_order_relaxed);
    // }}}
        }
    // v1 {{{
        // if (current >= static_cast<U>(B)) {
        //     current = current % static_cast<U>(B);
        // }
    // }}} v1
    // v2,3 {{{
        while (current >= static_cast<U>(B)) current -= static_cast<U>(B);
    // }}} v2,3
        return current;
    }

    /** Pre-increment iteration **/
    template<size_t B, any_atomic_uint T, typename U = T::value_type>
    requires (B > 1)
    U iterate_pre_inc(T & value) noexcept {
        assert(value >= 0);
        U current { value.load(std::memory_order_relaxed) };
        U next;
        do {
    // v1 {{{
            // next = (current + 1) % static_cast<U>(B);
    // }}} v1
    // v2 {{{
            next = current + 1;
            while (next >= static_cast<U>(B)) next -= static_cast<U>(B);
    // }}} v2
        } while (!value.compare_exchange_weak(current, next, std::memory_order_relaxed));
        return next;
    }
}
