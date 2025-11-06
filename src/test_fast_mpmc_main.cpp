// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "common.hpp"
#include "mpmc_config.hpp"
#include "messages.hpp"
#include <xtxn/fast_mpmc_queue.hpp>
#include <cassert>
#include <cstdlib>
#include <atomic>
#include <iostream>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <thread>
#include <latch>

using gp = xtxn::queue_growth_policy;

const std::unordered_map<gp, std::string_view> gp_labels {
    { gp::call, "call" },
    { gp::round, "round" },
    { gp::step, "step" },
};

template<int32_t S, int32_t L, int32_t A = 10, gp G = gp::round>
void queue_test(
    std::stringstream & stream,
    bool & ok,
    const int64_t items,
    const unsigned producers,
    const unsigned consumers
) {
    xtxn::fast_mpmc_queue<int_fast64_t, S, L, true, A, G> queue {};
    std::vector<std::jthread> pool {};
    std::latch exit_latch { producers + consumers + 1 };
    std::atomic_int_fast64_t pro_time { 0 };
    std::atomic_int_fast64_t pro_successes { 0 };
    std::atomic_int_fast64_t pro_fails { 0 };
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
                    auto slot = queue.consumer_slot();
                    auto t2 = std::chrono::steady_clock::now();
                    auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                    con_time.fetch_add(t3, std::memory_order_acq_rel);
                    if (slot) {
                        result.fetch_add(*slot, std::memory_order_acq_rel);
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
            [& exit_latch, & queue, & counter, & pro_time, & pro_successes , & pro_fails] {
                int_fast64_t value { counter.fetch_sub(1, std::memory_order_acq_rel) };
                while (value > 0) {
                    auto t1 = std::chrono::steady_clock::now();
                    auto slot = queue.producer_slot();
                    auto t2 = std::chrono::steady_clock::now();
                    auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                    pro_time.fetch_add(t3, std::memory_order_acq_rel);
                    if (slot) {
                        *slot = value;
                        value = counter.fetch_sub(1, std::memory_order_acq_rel);
                        pro_successes.fetch_add(1, std::memory_order_acq_rel);
                    } else {
                        pro_fails.fetch_add(1, std::memory_order_acq_rel);
                        std::this_thread::yield();
                    }
                }
                exit_latch.arrive_and_wait();
            }
        );
    }

    while (counter.load() > 0 || con_successes.load() < items) {
        std::this_thread::yield();
    }
    queue.stop();
    exit_latch.arrive_and_wait();

    assert(queue.empty());

    auto t2 = std::chrono::steady_clock::now();
    auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

    ok = result.load() == ((items * (items + 1)) >> 1);

    summary_a(stream, items);
    summary_b(stream, gp_labels.at(G), decltype(queue)::c_default_attempts);
    summary_c(stream, producers, pro_time, pro_successes, pro_fails, consumers, con_time, con_successes, con_fails);
    summary_d(stream, queue.capacity(), decltype(queue)::c_block_size, decltype(queue)::c_max_capacity);
    summary_e(stream, ok, t3);
}

template<int32_t S, int32_t L, int32_t A = 10, gp G = gp::call>
void queue_test(const int64_t items, const unsigned producers, const unsigned consumers, std::string_view separator) {
    std::stringstream str {};
    bool ok {};
    queue_test<S, L, A, G>(str, ok, items, producers, consumers);
    std::cout << str.str() << separator;
}

int main(int, char **) {
    std::cout << thick_separator << "   FAST MPMC QUEUE TEST\n" << prelim_test;

    for (int i = pre_test_iters; i; --i) {
        std::stringstream str {};
        bool ok {};
        queue_test<50, 5'000>(str, ok, pre_test_items, producers_d, consumers_d);
        if (!ok) {
            std::cout << has_failed << thin_separator << str.str() << thick_separator;
            return EXIT_SUCCESS;
        }
    }

    std::cout << is_complete;

    queue_test<1'000, 10'000>(100ll,     producers_d, consumers_d, thin_separator);
    queue_test<1'000, 10'000>(1'000ll,   producers_d, consumers_d, thin_separator);
    queue_test<1'000, 10'000>(10'000ll,  producers_d, consumers_d, thin_separator);
    queue_test<1'000, 10'000>(100'000ll, producers_d, consumers_d, thick_separator);

#ifndef _DEBUG

    std::cout << diff_size_and_attempts;

    queue_test<10,    10'000, 1  >(1'000'000ll, producers_a, consumers_a, thin_separator);
    queue_test<10,    10'000, 100>(1'000'000ll, producers_a, consumers_a, thin_separator);
    queue_test<100,   10'000, 1  >(1'000'000ll, producers_a, consumers_a, thin_separator);
    queue_test<100,   10'000, 100>(1'000'000ll, producers_a, consumers_a, thin_separator);
    queue_test<1'000, 10'000, 1  >(1'000'000ll, producers_a, consumers_a, thin_separator);
    queue_test<1'000, 10'000, 100>(1'000'000ll, producers_a, consumers_a, thick_separator);

    std::cout << diff_workers_and_policies;

    queue_test<100, 10'000, 10, gp::call >(1'000'000ll, producers_a, consumers_a, thin_separator);
    queue_test<100, 10'000, 10, gp::round>(1'000'000ll, producers_a, consumers_a, thin_separator);
    queue_test<100, 10'000, 10, gp::step >(1'000'000ll, producers_a, consumers_a, thin_separator);

    queue_test<100, 10'000, 10, gp::call >(1'000'000ll, producers_b, consumers_b, thin_separator);
    queue_test<100, 10'000, 10, gp::round>(1'000'000ll, producers_b, consumers_b, thin_separator);
    queue_test<100, 10'000, 10, gp::step >(1'000'000ll, producers_b, consumers_b, thin_separator);

    queue_test<100, 10'000, 10, gp::call >(1'000'000ll, producers_c, consumers_c, thin_separator);
    queue_test<100, 10'000, 10, gp::round>(1'000'000ll, producers_c, consumers_c, thin_separator);
    queue_test<100, 10'000, 10, gp::step >(1'000'000ll, producers_c, consumers_c, thin_separator);

    queue_test<100, 10'000, 10, gp::call >(1'000'000ll, producers_d, consumers_d, thin_separator);
    queue_test<100, 10'000, 10, gp::round>(1'000'000ll, producers_d, consumers_d, thin_separator);
    queue_test<100, 10'000, 10, gp::step >(1'000'000ll, producers_d, consumers_d, thick_separator);

#endif

    std::cout << all_tests_passed;

    return EXIT_SUCCESS;
}
