// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <atomic>
#include <thread>

namespace xtxn {
    class color_barrier {
        friend class red_lock;
        friend class green_lock;

        std::atomic_uint_fast64_t m_red_counter {};
        std::atomic_uint_fast64_t m_green_counter {};

    public:
        color_barrier() noexcept = default;
        color_barrier(const color_barrier &) = delete;
        color_barrier(color_barrier &&) = delete;
        ~color_barrier() noexcept = default;

        color_barrier & operator=(const color_barrier &) = delete;
        color_barrier & operator=(color_barrier &&) = delete;
    };

    class red_lock {
        color_barrier & m_color_barrier;

    public:
        red_lock() = delete;
        red_lock(const red_lock &) = delete;
        red_lock(red_lock &&) = delete;

        explicit red_lock(color_barrier & barrier) noexcept
        : m_color_barrier { barrier } {
            while (m_color_barrier.m_green_counter.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            m_color_barrier.m_red_counter.fetch_add(1, std::memory_order_acq_rel);
        }

        ~red_lock() noexcept {
            m_color_barrier.m_red_counter.fetch_sub(1, std::memory_order_acq_rel);
        }

        red_lock & operator=(const red_lock &) = delete;
        red_lock & operator=(red_lock &&) = delete;
    };

    class green_lock {
        color_barrier & m_color_barrier;

    public:
        green_lock() = delete;
        green_lock(const green_lock &) = delete;
        green_lock(green_lock &&) = delete;

        explicit green_lock(color_barrier & barrier) noexcept
        : m_color_barrier { barrier } {
            while (m_color_barrier.m_red_counter.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            m_color_barrier.m_green_counter.fetch_add(1, std::memory_order_acq_rel);
        }

        ~green_lock() noexcept {
            m_color_barrier.m_green_counter.fetch_sub(1, std::memory_order_acq_rel);
        }

        green_lock & operator=(const green_lock &) = delete;
        green_lock & operator=(green_lock &&) = delete;
    };
}
