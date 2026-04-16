// Copyright (c) 2025-2026 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <atomic>
#include <thread>
#if defined(_MSC_VER) && !defined(__clang__)
#   include <intrin.h> // NOLINT
#else
#   include <immintrin.h> // NOLINT
#endif

namespace xtxn {
    enum class spin { pause, yield_thread, wait_flag, active };

    template<spin P = spin::pause>
    class spinlock {
        std::atomic_flag m_flag {};

    public:
        spinlock() = default;
        spinlock(const spinlock &) = delete;
        spinlock(spinlock &&) = delete;
        ~spinlock() noexcept = default;

        spinlock & operator=(const spinlock &) = delete;
        spinlock & operator=(spinlock &&) = delete;

        void lock() noexcept {
            while (m_flag.test_and_set(std::memory_order_acquire)) {
                if constexpr (P == spin::pause) {
                    _mm_pause();
                } else if constexpr (P == spin::yield_thread) {
                    std::this_thread::yield();
                } else if constexpr (P == spin::wait_flag) {
                    m_flag.wait(true, std::memory_order_relaxed);
                }
            }
        }

        bool try_lock() noexcept {
            return !m_flag.test_and_set(std::memory_order_acquire);
        }

        void unlock() noexcept {
            m_flag.clear(std::memory_order_release);
            if constexpr (P == spin::wait_flag) {
                m_flag.notify_one();
            }
        }
    };

    template class spinlock<spin::pause>;
    template class spinlock<spin::yield_thread>;
    template class spinlock<spin::wait_flag>;
    template class spinlock<spin::active>;
}
