// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include "types.hpp"
#include <cassert>
#include <algorithm>
#include <thread>

namespace test::config {
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
        const unsigned cores;
        const config_set set_a;
        const config_set set_b;
        const config_set set_c;
        const config_set set_d;

        mpsc()
        : prelim {},
          cores { std::thread::hardware_concurrency() },
          set_a { 1, 1 },
          set_b { std::max(1u, cores - 1), 1 },
          set_c { cores, 1 },
          set_d { cores << 1, 1 } {}

        mpsc(const mpsc &) = delete;
        mpsc(mpsc &&) = delete;
        ~mpsc() = default;

        mpsc & operator=(const mpsc &) = delete;
        mpsc & operator=(mpsc &&) = delete;
    };

    struct mpmc : public prelim {
        const unsigned cores;
        const config_set set_a;
        const config_set set_b;
        const config_set set_c;
        const config_set set_d;

        mpmc()
        : prelim {},
          cores { std::thread::hardware_concurrency() },
          set_a { proportion(cores, cores >> 1) },
          set_b { proportion(cores, static_cast<unsigned>(static_cast<double>(cores) / 3.0 * 2.0)) },
          set_c { same(cores) },
          set_d { same(cores << 1) } {}

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
