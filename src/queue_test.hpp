// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include "messages.hpp"
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <vector>
#include <thread>
#include <latch>
#include <iostream>

using item_type = int_fast64_t;

template<class T, typename U = item_type>
concept queue_type = requires(T t, U u) {
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
            while (value > 0) {
                auto t1 = std::chrono::steady_clock::now();
                queue.enqueue(value);
                auto t2 = std::chrono::steady_clock::now();
                auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                time.fetch_add(t3, std::memory_order_acq_rel);
                value = counter.fetch_sub(1, std::memory_order_acq_rel);
                successes.fetch_add(1, std::memory_order_acq_rel);
            }
            latch.arrive_and_wait();
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
                auto t1 = std::chrono::steady_clock::now();
                auto item = queue.dequeue();
                auto t2 = std::chrono::steady_clock::now();
                auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                time.fetch_add(t3, std::memory_order_acq_rel);
                if (item) {
                    result.fetch_add(*item, std::memory_order_acq_rel);
                    successes.fetch_add(1, std::memory_order_acq_rel);
                } else {
                    fails.fetch_add(1, std::memory_order_acq_rel);
                    std::this_thread::yield();
                }
            }
            latch.arrive_and_wait();
        };
}

template<queue_type T>
int queue_test(std::stringstream & stream, const item_type items, const test_config & config) {
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

    auto t1 = std::chrono::steady_clock::now();

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

    auto t2 = std::chrono::steady_clock::now();
    auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    int exit_code { result.load() == ((items * (items + 1)) >> 1) ? EXIT_SUCCESS : EXIT_FAILURE };

    summary_a(stream, items);
    summary_c(stream, config.first, pro_time, pro_successes, 0, config.second, con_time, con_successes, con_fails);
    summary_e(stream, exit_code == EXIT_SUCCESS, t3);

    return exit_code;
}

template<queue_type T>
void queue_test(const item_type items, const test_config & config, std::string_view separator) {
    std::stringstream str {};
    int result { queue_test<T>(str, items, config) };
    std::cout << str.str() << separator;
    if (result != EXIT_SUCCESS) {
        std::exit(result);
    }
}

template<queue_type T>
void queue_test(const int iters, const item_type items, const test_config & config) {
    for (int i { iters }; i; --i) {
        std::stringstream str {};
        int result { queue_test<T>(str, items, config) };
        if (result != EXIT_SUCCESS) {
            std::cout << has_failed << thin_separator << str.str() << thick_separator;
            std::exit(result);
        }
    }
}

template<queue_type T>
void queue_test(std::string_view test_name, auto test_config) {
    std::cout << thick_separator << "   " << test_name << '\n' << prelim_test;

    queue_test<T>(test_config.prelim_test_iters, test_config.prelim_test_items, test_config.set_d);

    std::cout << is_complete;

    queue_test<T>(100ll, test_config.set_d, thin_separator);
    queue_test<T>(1'000ll, test_config.set_d, thin_separator);
    queue_test<T>(10'000ll, test_config.set_d, thin_separator);
    queue_test<T>(100'000ll, test_config.set_d, thick_separator);

#ifndef _DEBUG

    std::cout << diff_workers;

    queue_test<T>(1'000'000ll, test_config.set_a, thin_separator);
    queue_test<T>(1'000'000ll, test_config.set_b, thin_separator);
    queue_test<T>(1'000'000ll, test_config.set_c, thin_separator);
    queue_test<T>(1'000'000ll, test_config.set_d, thick_separator);

#endif

    std::cout << all_tests_passed;
}
