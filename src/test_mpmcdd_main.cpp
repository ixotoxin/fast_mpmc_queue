// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "common.hpp"
#include "mpmc_config.hpp"
#include "messages.hpp"
#include <xtxn/mpmcdd_queue.hpp>

#include <cstdlib>
#include <iostream>
#include <chrono>
#include <atomic>
#include <vector>
#include <thread>
#include <latch>

void queue_test(
    std::stringstream & stream,
    bool & ok,
    const int64_t items,
    const unsigned producers,
    const unsigned consumers
) {
    xtxn::mpmcdd_queue<int_fast64_t> queue {};
    std::vector<std::jthread> pool {};
    std::latch exit_latch { producers + consumers + 1 };
    std::atomic_int_fast64_t pro_time { 0 };
    std::atomic_int_fast64_t pro_successes { 0 };
    std::atomic_int_fast64_t con_time { 0 };
    std::atomic_int_fast64_t con_successes { 0 };
    std::atomic_int_fast64_t con_fails { 0 };
    std::atomic_int_fast64_t counter { items };
    std::atomic_int_fast64_t result { 0 };

    auto t1 = std::chrono::steady_clock::now();

    for (unsigned i = consumers; i; --i) {
        pool.emplace_back(
            [& exit_latch, & queue, & result, & con_time, & con_successes, & con_fails] {
                while (queue.consuming()) {
                    auto t1 = std::chrono::steady_clock::now();
                    auto item = queue.dequeue();
                    auto t2 = std::chrono::steady_clock::now();
                    auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                    con_time.fetch_add(t3, std::memory_order_acq_rel);
                    if (item) {
                        result.fetch_add(*item, std::memory_order_acq_rel);
                        con_successes.fetch_add(1, std::memory_order_acq_rel);
                    } else {
                        con_fails.fetch_add(1, std::memory_order_acq_rel);
                        std::this_thread::yield();
                    }
                }
                exit_latch.arrive_and_wait();
            }
        );
    }

    for (unsigned i = producers; i; --i) {
        pool.emplace_back(
            [& exit_latch, & queue, & counter, & pro_time, & pro_successes] {
                int_fast64_t value { counter.fetch_sub(1, std::memory_order_acq_rel) };
                while (value > 0) {
                    auto t1 = std::chrono::steady_clock::now();
                    queue.enqueue(value);
                    auto t2 = std::chrono::steady_clock::now();
                    auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                    pro_time.fetch_add(t3, std::memory_order_acq_rel);
                    value = counter.fetch_sub(1, std::memory_order_acq_rel);
                    pro_successes.fetch_add(1, std::memory_order_acq_rel);
                }
                exit_latch.arrive_and_wait();
            }
        );
    }

    while (counter.load() > 0 /*|| !queue.empty()*/ || con_successes.load() < items) {
        std::this_thread::yield();
    }
    queue.stop();
    exit_latch.arrive_and_wait();

    auto t2 = std::chrono::steady_clock::now();
    auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

    ok = result.load() == ((items * (items + 1)) >> 1);

    summary_a(stream, items);
    summary_c(stream, producers, pro_time, pro_successes, 0, consumers, con_time, con_successes, con_fails);
    summary_e(stream, ok, t3);
}

void queue_test(const int64_t items, const unsigned producers, const unsigned consumers, std::string_view separator) {
    std::stringstream str {};
    bool ok {};
    queue_test(str, ok, items, producers, consumers);
    std::cout << str.str() << separator;
}

int main(int, char **) {
    std::cout << thick_separator << "   CLASSIC MPMC QUEUE TEST (DEFERRED DELETION)\n" << prelim_test;

    for (int i = pre_test_iters; i; --i) {
        std::stringstream str {};
        bool ok {};
        queue_test(str, ok, pre_test_items, producers_d, consumers_d);
        if (!ok) {
            std::cout << has_failed << thin_separator << str.str() << thick_separator;
            return EXIT_SUCCESS;
        }
    }

    std::cout << is_complete;

    queue_test(100ll,    producers_d, consumers_d, thin_separator);
    queue_test(1'000ll,  producers_d, consumers_d, thin_separator);
    queue_test(10'000ll, producers_d, consumers_d, thick_separator);

#ifndef _DEBUG

    std::cout << diff_workers;

    queue_test(1'000'000ll, producers_a, consumers_a, thin_separator);
    queue_test(1'000'000ll, producers_b, consumers_b, thin_separator);
    queue_test(1'000'000ll, producers_c, consumers_c, thin_separator);
    queue_test(1'000'000ll, producers_d, consumers_d, thick_separator);

#endif

    std::cout << all_tests_passed;

    return EXIT_SUCCESS;
}
