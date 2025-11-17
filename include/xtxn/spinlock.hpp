// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <atomic>
#include <thread>

namespace xtxn {
    enum class spin { active, yield_thread, wait_flag };

    template<spin P = spin::active>
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
                if constexpr (P == spin::yield_thread) {
                    std::this_thread::yield();
                } else if constexpr (P == spin::wait_flag) {
                    m_flag.wait(true, std::memory_order_relaxed);
                }
            }
        }

        void unlock() noexcept {
            m_flag.clear(std::memory_order_release);
            if constexpr (P == spin::wait_flag) {
                m_flag.notify_one();
            }
        }
    };

    template<spin P>
    class scoped_lock {
        spinlock<P> & m_spinlock {};

    public:
        scoped_lock() = delete;
        scoped_lock(const scoped_lock &) = delete;
        scoped_lock(scoped_lock &&) = delete;
        explicit scoped_lock(spinlock<P> & sl) noexcept : m_spinlock { sl } { m_spinlock.lock(); }
        ~scoped_lock() noexcept { m_spinlock.unlock(); }

        scoped_lock & operator=(const scoped_lock &) = delete;
        scoped_lock & operator=(scoped_lock &&) = delete;
    };
}
