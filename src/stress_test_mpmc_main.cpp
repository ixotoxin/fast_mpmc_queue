// Copyright (c) 2025-2026 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "init.hpp"
#include "config.hpp"
#include "messages.hpp"
#include <xtxn/dynamic_fast_mpmc_queue.hpp>
#include <xtxn/static_fast_mpmc_queue.hpp>
#include <xtxn/mpmc_queue.hpp>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <vector>
#include <thread>
#include <latch>
#include <iostream>

int main(int, char **) {
    using namespace xtxn;

    init::console();
    init::profiler();

    const unsigned producers { std::thread::hardware_concurrency() << 2 };
    const unsigned consumers { producers };
#ifdef _DEBUG
    constexpr int64_t items { 100'000 };
#else
    constexpr int64_t items { 10'000'000 };
#endif

    {
        std::stringstream str {};
        dynamic_fast_mpmc_queue<test::item_type, 100, 400, true, 1, queue_growth_policy::call> queue {};
        std::vector<std::jthread> pool {};
        std::latch latch { producers + consumers + 1 };
        std::atomic_int_fast64_t consumed { 0 };
        std::atomic_int_fast64_t counter { items };
        std::atomic_int_fast64_t result { 0 };

        std::cout << thick_separator << "\n   DYNAMIC FAST MPMC QUEUE\n";

        const auto t1 = std::chrono::steady_clock::now();

        for (unsigned i { consumers }; i; --i) {
            pool.emplace_back(
                [& queue, & result, & consumed, & latch] {
                    while (queue.consuming()) {
                        if (auto slot = queue.consumer_slot(); slot) {
                            result.fetch_add(*slot, std::memory_order_acq_rel);
                            consumed.fetch_add(1, std::memory_order_acq_rel);
                        } else {
                            std::this_thread::yield();
                        }
                    }
                    latch.count_down();
                }
            );
        }

        for (unsigned i { producers }; i; --i) {
            pool.emplace_back(
                [& queue, & counter, & latch] {
                    int_fast64_t value { counter.fetch_sub(1, std::memory_order_acq_rel) };
                    while (queue.producing() && value > 0) {
                        if (auto slot = queue.producer_slot(); slot) {
                            *slot = value;
                            value = counter.fetch_sub(1, std::memory_order_acq_rel);
                        } else {
                            std::this_thread::yield();
                        }
                    }
                    latch.count_down();
                }
            );
        }

        while (counter.load() > 0 || consumed.load() < items) {
            std::this_thread::yield();
        }
        queue.stop();
        latch.arrive_and_wait();

        assert(queue.empty());

        const auto t2 = std::chrono::steady_clock::now();
        const auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        summary_e(str, result.load() == (items * (items + 1)) >> 1, t3);
        std::cout << str.str() << thick_separator;
    }

    {
        std::stringstream str {};
        static_fast_mpmc_queue<test::item_type, 50, true, 1> queue {};
        std::vector<std::jthread> pool {};
        std::latch latch { producers + consumers + 1 };
        std::atomic_int_fast64_t consumed { 0 };
        std::atomic_int_fast64_t counter { items };
        std::atomic_int_fast64_t result { 0 };

        std::cout << "\n   STATIC FAST MPMC QUEUE\n";

        const auto t1 = std::chrono::steady_clock::now();

        for (unsigned i { consumers }; i; --i) {
            pool.emplace_back(
                [& queue, & result, & consumed, & latch] {
                    while (queue.consuming()) {
                        if (auto slot = queue.consumer_slot(); slot) {
                            result.fetch_add(*slot, std::memory_order_acq_rel);
                            consumed.fetch_add(1, std::memory_order_acq_rel);
                        } else {
                            std::this_thread::yield();
                        }
                    }
                    latch.count_down();
                }
            );
        }

        for (unsigned i { producers }; i; --i) {
            pool.emplace_back(
                [& queue, & counter, & latch] {
                    int_fast64_t value { counter.fetch_sub(1, std::memory_order_acq_rel) };
                    while (queue.producing() && value > 0) {
                        if (auto slot = queue.producer_slot(); slot) {
                            *slot = value;
                            value = counter.fetch_sub(1, std::memory_order_acq_rel);
                        } else {
                            std::this_thread::yield();
                        }
                    }
                    latch.count_down();
                }
            );
        }

        while (counter.load() > 0 || consumed.load() < items) {
            std::this_thread::yield();
        }
        queue.stop();
        latch.arrive_and_wait();

        assert(queue.empty());

        const auto t2 = std::chrono::steady_clock::now();
        const auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        summary_e(str, result.load() == (items * (items + 1)) >> 1, t3);
        std::cout << str.str() << thick_separator;
    }

    {
        std::stringstream str {};
        mpmc_queue<test::item_type, 500, true, 1000> queue {};
        std::vector<std::jthread> pool {};
        std::latch latch { producers + consumers + 1 };
        std::atomic_int_fast64_t consumed { 0 };
        std::atomic_int_fast64_t counter { items };
        std::atomic_int_fast64_t result { 0 };

        std::cout << "\n   CLASSIC MPMC QUEUE\n";

        const auto t1 = std::chrono::steady_clock::now();

        for (unsigned i { consumers }; i; --i) {
            pool.emplace_back(
                [& queue, & result, & consumed, & latch] {
                    while (queue.consuming()) {
                        if (auto item = queue.dequeue(); item) {
                            result.fetch_add(*item, std::memory_order_acq_rel);
                            consumed.fetch_add(1, std::memory_order_acq_rel);
                        } else {
                            std::this_thread::yield();
                        }
                    }
                    queue.escape();
                    latch.count_down();
                }
            );
        }

        for (unsigned i { producers }; i; --i) {
            pool.emplace_back(
                [& queue, & counter, & latch] {
                    int_fast64_t value { counter.fetch_sub(1, std::memory_order_acq_rel) };
                    while (queue.producing() && value > 0) {
                        queue.enqueue(value);
                        value = counter.fetch_sub(1, std::memory_order_acq_rel);
                    }
                    queue.escape();
                    latch.count_down();
                }
            );
        }

        while (counter.load() > 0 || consumed.load() < items) {
            std::this_thread::yield();
        }
        queue.stop();
        latch.arrive_and_wait();

        assert(queue.empty());

        const auto t2 = std::chrono::steady_clock::now();
        const auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        summary_e(str, result.load() == (items * (items + 1)) >> 1, t3);
        std::cout << str.str() << thick_separator;
    }

    return EXIT_SUCCESS;
}
