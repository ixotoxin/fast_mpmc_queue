// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include "messages.hpp"
#include "types.hpp"
#include <xtxn/fast_mpmc_queue.hpp>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <thread>
#include <latch>
#include <iostream>

namespace test {
    namespace {
        using queue_gp = xtxn::queue_growth_policy;

        template<int S, int L, int A = 10, queue_gp G = queue_gp::round>
        using queue = xtxn::fast_mpmc_queue<item_type, S, L, true, A, G>;

        inline const std::unordered_map<queue_gp, std::string_view> gp_labels {
            { queue_gp::call,  "call" },
            { queue_gp::round, "round" },
            { queue_gp::step,  "step" },
        };

        auto create_producer(
            xtxn::fast_mpmc_queue_tc auto & queue,
            std::atomic<item_type> & counter,
            std::atomic_int_fast64_t & time,
            std::atomic_int_fast64_t & successes,
            std::atomic_int_fast64_t & fails,
            std::latch & latch
        ) {
            return
                [& queue, & counter, & time, & successes, & fails, & latch] {
                    item_type value { counter.fetch_sub(1, std::memory_order_acq_rel) };
                    while (value > 0) {
                        auto t1 = std::chrono::steady_clock::now();
                        auto slot = queue.producer_slot();
                        auto t2 = std::chrono::steady_clock::now();
                        auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                        time.fetch_add(t3, std::memory_order_acq_rel);
                        if (slot) {
                            *slot = value;
                            value = counter.fetch_sub(1, std::memory_order_acq_rel);
                            successes.fetch_add(1, std::memory_order_acq_rel);
                        } else {
                            fails.fetch_add(1, std::memory_order_acq_rel);
                            std::this_thread::yield();
                        }
                    }
                    latch.arrive_and_wait();
                };
        }

        auto create_consumer(
            xtxn::fast_mpmc_queue_tc auto & queue,
            std::atomic<item_type> & result,
            std::atomic_int_fast64_t & time,
            std::atomic_int_fast64_t & successes,
            std::atomic_int_fast64_t & fails,
            std::latch & latch
        ) {
            return
                [& queue, & result, & time, & successes, & fails, & latch] {
                    while (queue.consuming()) {
                        auto t1 = std::chrono::steady_clock::now();
                        auto slot = queue.consumer_slot();
                        auto t2 = std::chrono::steady_clock::now();
                        auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                        time.fetch_add(t3, std::memory_order_acq_rel);
                        if (slot) {
                            result.fetch_add(*slot, std::memory_order_acq_rel);
                            successes.fetch_add(1, std::memory_order_acq_rel);
                        } else {
                            fails.fetch_add(1, std::memory_order_acq_rel);
                            std::this_thread::yield();
                        }
                    }
                    latch.arrive_and_wait();
                };
        }
    }

    template<xtxn::fast_mpmc_queue_tc T>
    int perform(std::stringstream & stream, const item_type items, const config_set & config) {
        T queue {};
        std::vector<std::jthread> pool {};
        std::latch latch { config.first + config.second + 1 };
        std::atomic_int_fast64_t pro_time { 0 };
        std::atomic_int_fast64_t pro_successes { 0 };
        std::atomic_int_fast64_t pro_fails { 0 };
        std::atomic_int_fast64_t con_time { 0 };
        std::atomic_int_fast64_t con_successes { 0 };
        std::atomic_int_fast64_t con_fails { 0 };
        std::atomic<item_type> counter { items };
        std::atomic<item_type> result { 0 };

        auto t1 = std::chrono::steady_clock::now();

        for (unsigned i { config.second }; i; --i) {
            pool.emplace_back(create_consumer(queue, result, con_time, con_successes, con_fails, latch));
        }

        for (unsigned i { config.first }; i; --i) {
            pool.emplace_back(create_producer(queue, counter, pro_time, pro_successes, pro_fails, latch));
        }

        while (counter.load() > 0 || con_successes.load() < items) {
            std::this_thread::yield();
        }
        queue.stop();
        latch.arrive_and_wait();

        assert(queue.empty());

        auto t2 = std::chrono::steady_clock::now();
        auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        int exit_code { result.load() == ((items * (items + 1)) >> 1) ? EXIT_SUCCESS : EXIT_FAILURE };

        summary_a(stream, items);
        summary_b(stream, gp_labels.at(T::c_growth_policy), T::c_default_attempts);

        summary_c(
            stream, config.first, pro_time, pro_successes, pro_fails,
            config.second, con_time, con_successes, con_fails
        );

        summary_d(stream, queue.capacity(), T::c_block_size, T::c_max_capacity);
        summary_e(stream, exit_code == EXIT_SUCCESS, t3);

        return exit_code;
    }

    template<xtxn::fast_mpmc_queue_tc T>
    void perform(const item_type items, const config_set & config, std::string_view separator) {
        std::stringstream str {};
        int result { perform<T>(str, items, config) };
        std::cout << str.str() << separator;
        if (result != EXIT_SUCCESS) {
            std::exit(result);
        }
    }

    template<xtxn::fast_mpmc_queue_tc T>
    void perform(const int iters, const item_type items, const config_set & config) {
        for (int i { iters }; i; --i) {
            std::stringstream str {};
            int result { perform<T>(str, items, config) };
            if (result != EXIT_SUCCESS) {
                std::cout << has_failed << thin_separator << str.str() << thick_separator;
                std::exit(result);
            }
        }
    }

    void perform(std::string_view test_name, auto test_config) {
        std::cout << thick_separator << "   " << test_name << '\n' << prelim_test;

        perform<queue<50, 5'000>>(test_config.prelim_test_iters, test_config.prelim_test_items, test_config.set_d);

        std::cout << is_complete;

        perform<queue<1'000, 10'000>>(100ll, test_config.set_d, thin_separator);
        perform<queue<1'000, 10'000>>(1'000ll, test_config.set_d, thin_separator);
        perform<queue<1'000, 10'000>>(10'000ll, test_config.set_d, thin_separator);
        perform<queue<1'000, 10'000>>(100'000ll, test_config.set_d, thick_separator);

#ifndef _DEBUG

        std::cout << diff_size_and_attempts;

        perform<queue<10, 10'000, 1>>(1'000'000ll, test_config.set_a, thin_separator);
        perform<queue<10, 10'000, 100>>(1'000'000ll, test_config.set_a, thin_separator);
        perform<queue<100, 10'000, 1>>(1'000'000ll, test_config.set_a, thin_separator);
        perform<queue<100, 10'000, 100>>(1'000'000ll, test_config.set_a, thin_separator);
        perform<queue<1'000, 10'000, 1>>(1'000'000ll, test_config.set_a, thin_separator);
        perform<queue<1'000, 10'000, 100>>(1'000'000ll, test_config.set_a, thick_separator);

        std::cout << diff_workers_and_policies;

        perform<queue<100, 10'000, 10, queue_gp::call>>(1'000'000ll, test_config.set_a, thin_separator);
        perform<queue<100, 10'000, 10, queue_gp::round>>(1'000'000ll, test_config.set_a, thin_separator);
        perform<queue<100, 10'000, 10, queue_gp::step>>(1'000'000ll, test_config.set_a, thin_separator);

        perform<queue<100, 10'000, 10, queue_gp::call>>(1'000'000ll, test_config.set_b, thin_separator);
        perform<queue<100, 10'000, 10, queue_gp::round>>(1'000'000ll, test_config.set_b, thin_separator);
        perform<queue<100, 10'000, 10, queue_gp::step>>(1'000'000ll, test_config.set_b, thin_separator);

        perform<queue<100, 10'000, 10, queue_gp::call>>(1'000'000ll, test_config.set_c, thin_separator);
        perform<queue<100, 10'000, 10, queue_gp::round>>(1'000'000ll, test_config.set_c, thin_separator);
        perform<queue<100, 10'000, 10, queue_gp::step>>(1'000'000ll, test_config.set_c, thin_separator);

        perform<queue<100, 10'000, 10, queue_gp::call>>(1'000'000ll, test_config.set_d, thin_separator);
        perform<queue<100, 10'000, 10, queue_gp::round>>(1'000'000ll, test_config.set_d, thin_separator);
        perform<queue<100, 10'000, 10, queue_gp::step>>(1'000'000ll, test_config.set_d, thick_separator);

#endif

        std::cout << all_tests_passed;
    }
}
