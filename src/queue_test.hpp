// Copyright (c) 2025-2026 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include "messages.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <vector>
#include <thread>
#include <latch>
#include <iostream>

namespace test {
    template<class T, typename U = item_type>
    concept queue_type
        = requires(T t, U u) {
            { t.empty() } -> std::same_as<bool>;
            { t.producing() } -> std::same_as<bool>;
            { t.consuming() } -> std::same_as<bool>;
            { t.enqueue(u) } -> std::same_as<bool>;
            { t.dequeue() } -> std::same_as<std::unique_ptr<U>>;
            { t.shutdown() };
            { t.stop() };
        };

    auto create_producer(
        queue_type auto & queue,
        std::atomic<item_type> & counter,
        std::atomic_int_fast64_t & time,
        std::atomic_int_fast64_t & successes,
        std::latch & latch
    ) {
        return
            [& queue, & counter, & time, & successes, & latch] {
                item_type value { counter.fetch_sub(1, std::memory_order_acq_rel) };
                while (queue.producing() && value > 0) {
                    const auto t1 = std::chrono::steady_clock::now();
                    queue.enqueue(value);
                    const auto t2 = std::chrono::steady_clock::now();
                    const auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                    time.fetch_add(t3, std::memory_order_acq_rel);
                    value = counter.fetch_sub(1, std::memory_order_acq_rel);
                    successes.fetch_add(1, std::memory_order_acq_rel);
                }
                latch.count_down();
            };
    }

    auto create_consumer(
        queue_type auto & queue,
        std::atomic<item_type> & result,
        std::atomic_int_fast64_t & time,
        std::atomic_int_fast64_t & successes,
        std::atomic_int_fast64_t & fails,
        std::latch & latch
    ) {
        return
            [& queue, & result, & time, & successes, & fails, & latch] {
                while (queue.consuming()) {
                    const auto t1 = std::chrono::steady_clock::now();
                    auto item = queue.dequeue();
                    const auto t2 = std::chrono::steady_clock::now();
                    const auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                    time.fetch_add(t3, std::memory_order_acq_rel);
                    if (item) {
                        result.fetch_add(*item, std::memory_order_acq_rel);
                        successes.fetch_add(1, std::memory_order_acq_rel);
                    } else {
                        fails.fetch_add(1, std::memory_order_acq_rel);
                        std::this_thread::yield();
                    }
                }
                latch.count_down();
            };
    }

    template<queue_type T>
    int perform(std::stringstream & stream, const item_type items, const config_set & config) {
        T queue {};
        std::vector<std::jthread> pool {};
        std::latch latch { config.first + config.second + 1 };
        std::atomic_int_fast64_t pro_time { 0 };
        std::atomic_int_fast64_t pro_successes { 0 };
        std::atomic_int_fast64_t con_time { 0 };
        std::atomic_int_fast64_t con_successes { 0 };
        std::atomic_int_fast64_t con_fails { 0 };
        std::atomic<item_type> counter { items };
        std::atomic<item_type> result { 0 };

        const auto t1 = std::chrono::steady_clock::now();

        for (unsigned i { config.second }; i; --i) {
            pool.emplace_back(create_consumer(queue, result, con_time, con_successes, con_fails, latch));
        }

        for (unsigned i { config.first }; i; --i) {
            pool.emplace_back(create_producer(queue, counter, pro_time, pro_successes, latch));
        }

        while (counter.load() > 0 || con_successes.load() < items) {
            std::this_thread::yield();
        }
        queue.stop();
        latch.arrive_and_wait();

        assert(queue.empty());

        const auto t2 = std::chrono::steady_clock::now();
        const auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        const int exit_code { result.load() == (items * (items + 1)) >> 1 ? EXIT_SUCCESS : EXIT_FAILURE };

        summary_a(stream, items);
        summary_c(stream, config.first, pro_time, pro_successes, 0, config.second, con_time, con_successes, con_fails);
        summary_e(stream, exit_code == EXIT_SUCCESS, t3);

        return exit_code;
    }

    template<queue_type T>
    void perform(const item_type items, const config_set & config, const std::string_view separator) {
        std::stringstream str {};
        const int result { perform<T>(str, items, config) };
        std::cout << str.str() << separator;
        if (result != EXIT_SUCCESS) {
            std::cout << tests_failed;
            std::exit(result);
        }
    }

    template<queue_type T>
    void perform(const int iters, const item_type items, const config_set & config) {
        for (int i { iters }; i; --i) {
            std::stringstream str {};
            if (const int result { perform<T>(str, items, config) }; result != EXIT_SUCCESS) {
                std::cout << has_failed << thin_separator << str.str() << thick_separator << tests_failed;
                std::exit(result);
            }
        }
    }

    template<queue_type T>
    void perform(const std::string_view test_name, auto test_config) {
        std::cout << thick_separator << "   " << test_name << '\n' << prelim_test;

        perform<T>(test_config.prelim_test_iters, test_config.prelim_test_items, test_config.set_d);

        std::cout << is_complete;

        perform<T>(100ll, test_config.set_d, thin_separator);
        perform<T>(1'000ll, test_config.set_d, thin_separator);
        perform<T>(10'000ll, test_config.set_d, thin_separator);
        perform<T>(100'000ll, test_config.set_d, thick_separator);

#ifndef _DEBUG

        std::cout << diff_workers;

        perform<T>(1'000'000ll, test_config.set_a, thin_separator);
        perform<T>(1'000'000ll, test_config.set_b, thin_separator);
        perform<T>(1'000'000ll, test_config.set_c, thin_separator);
        perform<T>(1'000'000ll, test_config.set_d, thick_separator);

#endif

        std::cout << all_tests_passed;
    }
}
