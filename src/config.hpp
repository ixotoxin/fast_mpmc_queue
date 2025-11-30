// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include "types.hpp"
#include <cassert>
#include <algorithm>
#include <thread>

namespace test::config {
    inline unsigned baseline_concurrency() {
        return std::max(std::thread::hardware_concurrency(), 2u);
    }

    struct prelim {
#if defined(ENABLE_MEMORY_PROFILING)
        const int prelim_test_iters { 2 };
        const int64_t prelim_test_items { 100 };
#elif defined(_DEBUG)
        const int prelim_test_iters { 100 };
        const int64_t prelim_test_items { 100 };
#else
        const int prelim_test_iters { 1'000 };
        const int64_t prelim_test_items { 100 };
#endif
    };

    struct mpsc : public prelim {
        const unsigned concurrency;
        const config_set set_a;
        const config_set set_b;
        const config_set set_c;
        const config_set set_d;

        mpsc()
        : prelim {},
          concurrency { baseline_concurrency() },
          set_a { 1, 1 },
          set_b { std::max(1u, concurrency - 1), 1 },
          set_c { concurrency, 1 },
          set_d { concurrency << 1, 1 } {}

        mpsc(const mpsc &) = delete;
        mpsc(mpsc &&) = delete;
        ~mpsc() = default;

        mpsc & operator=(const mpsc &) = delete;
        mpsc & operator=(mpsc &&) = delete;
    };

    struct mpmc : public prelim {
        const unsigned concurrency;
        const config_set set_a;
        const config_set set_b;
        const config_set set_c;
        const config_set set_d;

        mpmc()
        : prelim {},
          concurrency { baseline_concurrency() },
          set_a { proportion(concurrency, concurrency >> 1) },
          set_b { proportion(concurrency, static_cast<unsigned>(static_cast<double>(concurrency) / 3.0 * 2.0)) },
          set_c { same(concurrency) },
          set_d { same(concurrency << 1) } {}

        mpmc(const mpmc &) = delete;
        mpmc(mpmc &&) = delete;
        ~mpmc() = default;

        mpmc & operator=(const mpmc &) = delete;
        mpmc & operator=(mpmc &&) = delete;

        static config_set same(unsigned workers) {
            return { std::max(1u, workers), std::max(1u, workers) };
        }

        static config_set proportion(unsigned total, unsigned producers) {
            assert(producers <= total);
            return { std::max(1u, producers), std::max(1u, total - std::max(1u, producers)) };
        }
    };
}
