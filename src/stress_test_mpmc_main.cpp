// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "common.hpp"
#include "messages.hpp"
#include <xtxn/fast_mpmc_queue.hpp>
#include <xtxn/mpmc_queue.hpp>
#include <cassert>
#include <cstdlib>
#include <atomic>
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <latch>

int main(int, char **) {
    const unsigned producers { std::thread::hardware_concurrency() << 2 };
    const unsigned consumers { producers };
#ifdef _DEBUG
    constexpr int64_t items { 1'000'000 };
#else
    constexpr int64_t items { 10'000'000 };
#endif

    {
        std::stringstream str {};
        xtxn::fast_mpmc_queue<int_fast64_t, 100, 400, true, 1, xtxn::queue_growth_policy::call> queue {};
        std::vector<std::jthread> pool {};
        std::latch exit_latch { producers + consumers + 1 };
        std::atomic_int_fast64_t consumed { 0 };
        std::atomic_int_fast64_t counter { items };
        std::atomic_int_fast64_t result { 0 };

        std::cout << thick_separator << "\n   FAST MPMC QUEUE\n";

        auto t1 = std::chrono::steady_clock::now();

        for (unsigned i = consumers; i; --i) {
            pool.emplace_back(
                [& exit_latch, & queue, & result, & consumed] {
                    while (queue.consuming()) {
                        auto slot = queue.consumer_slot();
                        if (slot) {
                            result.fetch_add(*slot, std::memory_order_acq_rel);
                            consumed.fetch_add(1, std::memory_order_acq_rel);
                        } else {
                            std::this_thread::yield();
                        }
                    }
                    exit_latch.arrive_and_wait();
                }
            );
        }

        for (unsigned i = producers; i; --i) {
            pool.emplace_back(
                [& exit_latch, & queue, & counter] {
                    int_fast64_t value { counter.fetch_sub(1, std::memory_order_acq_rel) };
                    while (value > 0) {
                        auto slot = queue.producer_slot();
                        if (slot) {
                            *slot = value;
                            value = counter.fetch_sub(1, std::memory_order_acq_rel);
                        } else {
                            std::this_thread::yield();
                        }
                    }
                    exit_latch.arrive_and_wait();
                }
            );
        }

        while (counter.load() > 0 || consumed.load() < items) {
            std::this_thread::yield();
        }
        queue.stop();
        exit_latch.arrive_and_wait();

        assert(queue.empty());

        auto t2 = std::chrono::steady_clock::now();
        auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        bool ok = result.load() == ((items * (items + 1)) >> 1);
        summary_e(str, ok, t3);
        std::cout << str.str() << thick_separator;
    }

    {
        std::stringstream str {};
        xtxn::mpmc_queue<int_fast64_t, 500, true, 1000> queue {};
        std::vector<std::jthread> pool {};
        std::latch exit_latch { producers + consumers + 1 };
        std::atomic_int_fast64_t consumed { 0 };
        std::atomic_int_fast64_t counter { items };
        std::atomic_int_fast64_t result { 0 };

        std::cout << "\n   CLASSIC MPMC QUEUE\n";

        auto t1 = std::chrono::steady_clock::now();

        for (unsigned i = consumers; i; --i) {
            pool.emplace_back(
                [& exit_latch, & queue, & result, & consumed] {
                    while (queue.consuming()) {
                        auto item = queue.dequeue();
                        if (item) {
                            result.fetch_add(*item, std::memory_order_acq_rel);
                            consumed.fetch_add(1, std::memory_order_acq_rel);
                        } else {
                            std::this_thread::yield();
                        }
                    }
                    queue.escape();
                    exit_latch.arrive_and_wait();
                }
            );
        }

        for (unsigned i = producers; i; --i) {
            pool.emplace_back(
                [& exit_latch, & queue, & counter] {
                    int_fast64_t value { counter.fetch_sub(1, std::memory_order_acq_rel) };
                    while (value > 0) {
                        queue.enqueue(value);
                        value = counter.fetch_sub(1, std::memory_order_acq_rel);
                    }
                    queue.escape();
                    exit_latch.arrive_and_wait();
                }
            );
        }

        while (counter.load() > 0 || consumed.load() < items) {
            std::this_thread::yield();
        }
        queue.stop();
        exit_latch.arrive_and_wait();

        assert(queue.empty());

        auto t2 = std::chrono::steady_clock::now();
        auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        bool ok = result.load() == ((items * (items + 1)) >> 1);
        summary_e(str, ok, t3);
        std::cout << str.str() << thick_separator;
    }

    return EXIT_SUCCESS;
}
