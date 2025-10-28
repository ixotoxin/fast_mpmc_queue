// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <thread>
#include <latch>
#include <xtxn/fast_mpmc_queue.hpp>

#if defined(_WIN32) && defined(_DEBUG)
#   include <crtdbg.h>
#endif

using gp = xtxn::growth_policy;

const std::unordered_map<gp, std::string_view> gp_labels {
    { gp::call, "call" },
    { gp::round, "round" },
    { gp::step, "step" },
};

template<int32_t S, int32_t L, gp G = gp::call>
requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max() / 2) && (L >= S)
void queue_test(
    std::stringstream & stream,
    bool & ok,
    const int64_t items,
    const int32_t attempts = 3,
    const unsigned producers = 5
) {
    assert((items & 1) == 0);
    xtxn::fast_mpmc_queue<int_fast64_t, S, L, G> queue { /*true,*/ attempts };
    std::vector<std::jthread> pool {};
    std::latch exit_latch { producers + 2 };
    std::atomic_int_fast64_t pro_time { 0 };
    std::atomic_int_fast64_t pro_successes { 0 };
    std::atomic_int_fast64_t pro_fails { 0 };
    std::atomic_int_fast64_t con_time { 0 };
    std::atomic_int_fast64_t con_successes { 0 };
    std::atomic_int_fast64_t con_fails { 0 };
    std::atomic_int_fast64_t counter { items };
    std::atomic_int_fast64_t result { 0 };

    auto t1 = std::chrono::steady_clock::now();

    pool.emplace_back(
        [& exit_latch, & queue, & result, & con_time, & con_successes, & con_fails] {
            while (queue.consuming()) {
                auto t1 = std::chrono::steady_clock::now();
                auto slot = queue.consumer_slot();
                auto t2 = std::chrono::steady_clock::now();
                con_time.fetch_add(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
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

    for (unsigned i = producers; i; --i) {
        pool.emplace_back(
            [& exit_latch, & queue, & counter, & pro_time, & pro_successes , & pro_fails] {
                int_fast64_t value { counter.fetch_sub(1, std::memory_order_acq_rel) };
                while (value > 0) {
                    auto t1 = std::chrono::steady_clock::now();
                    auto slot = queue.producer_slot();
                    auto t2 = std::chrono::steady_clock::now();
                    pro_time.fetch_add(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
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

    while (counter.load() > 0 /*|| !queue.empty()*/ || con_successes < items) {
        std::this_thread::yield();
    }
    queue.stop();
    exit_latch.arrive_and_wait();

    auto t2 = std::chrono::steady_clock::now();
    auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

    ok = result.load() == (items >> 1) * (items + 1);

    stream
        << "\n   Actual queue capacity: " << queue.capacity() << " slot (min: "
        << decltype(queue)::c_block_size << ", max: "
        << decltype(queue)::c_max_capacity << ")\n"
           "   Queue growth policy: allow at each " << gp_labels.at(G) << "\n"
           "   Producer slot acquire attempts: " << attempts << '\n'
        << std::fixed << std::setprecision(2)
        << "  -----------+------+--------------+-------------+-------------\n"
           "   WRK. TYPE | NUM. |  ACQU. TIME  | ACQU. SUCC. | ACQU. FAILS\n"
           "  -----------+------+--------------+-------------+-------------\n"
           "   Producers | "
        << std::setw(4) << producers << " | "
        << std::setw(9) << (static_cast<double>(pro_time) / 1'000) << " ms | "
        << std::setw(11) << pro_successes << " | "
        << std::setw(11) << pro_fails << "\n"
           "   Consumers |    1 | "
        << std::setw(9) << (static_cast<double>(con_time) / 1'000)  << " ms | "
        << std::setw(11) << con_successes  << " | "
        << std::setw(11) << con_fails  << "\n"
           "  -----------+------+--------------+-------------+-------------\n"
        //    "   Empty queue: " << (queue.capacity() == queue.free_slots() ? "Yes" : "No") << "\n"
           "   Control sum: " << (ok ? "OK" : "Invalid") << "\n"
           "   Real total time: " << (static_cast<double>(t3) / 1'000) << " ms\n\n"
           "=================================================================\n";
}

template<int32_t S, int32_t L, gp G = gp::call>
requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max() / 2) && (L >= S)
void queue_test(const int64_t items, const int32_t attempts = 3, const unsigned producers = 5) {
    std::stringstream str {};
    bool ok {};
    queue_test<S, L, G>(str, ok, items, attempts, producers);
    std::cout << str.str();
}

int main(int, char **) {
#ifdef _DEBUG
#   if defined(_WIN32) && (defined(_MSC_VER) || defined(__clang__))
    {
        constexpr auto _report_mode_ = /*_CRTDBG_MODE_DEBUG |*/ _CRTDBG_MODE_FILE /*| _CRTDBG_MODE_WNDW*/;
        ::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
        ::_CrtSetReportMode(_CRT_ASSERT, _report_mode_);
        ::_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        ::_CrtSetReportMode(_CRT_WARN, _report_mode_);
        ::_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        ::_CrtSetReportMode(_CRT_ERROR, _report_mode_);
        ::_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    }
#   endif
    constexpr int pre_test_iters { 200 };
#else
    constexpr int pre_test_iters { 2'000 };
#endif

    constexpr int32_t acquire_attempts { 10 };

    std::cout
        << "=================================================================\n"
           "   FAST UNSTABLE MPSC QUEUE TEST  \n"
           "=================================================================\n";

    for (int i = pre_test_iters; i; --i) {
        std::stringstream str {};
        bool ok {};
        queue_test<50, 5'000>(str, ok, 100, acquire_attempts);
        if (!ok) {
            std::cout << str.str();
            break;
        }
    }

    std::cout
        << "   The preliminary test is completed  \n"
           "=================================================================\n";

    queue_test<1'000, 10'000>(100, acquire_attempts);
    queue_test<1'000, 10'000>(1'000, acquire_attempts);
    queue_test<1'000, 10'000>(10'000, acquire_attempts);

#ifndef _DEBUG

    queue_test<10, 10'000>(1'000'000, 1);
    queue_test<100, 10'000>(1'000'000, 1);
    queue_test<1'000, 10'000>(1'000'000, 1);

    queue_test<10, 10'000>(1'000'000, acquire_attempts);
    queue_test<100, 10'000>(1'000'000, acquire_attempts);
    queue_test<1'000, 10'000>(1'000'000, acquire_attempts);

    unsigned producers = std::thread::hardware_concurrency() - 1;

    queue_test<10, 10'000, gp::call>(1'000'000, acquire_attempts, producers);
    queue_test<10, 10'000, gp::round>(1'000'000, acquire_attempts, producers);
    queue_test<10, 10'000, gp::step>(1'000'000, acquire_attempts, producers);

    queue_test<1'000, 10'000, gp::call>(1'000'000, acquire_attempts, producers);
    queue_test<1'000, 10'000, gp::round>(1'000'000, acquire_attempts, producers);
    queue_test<1'000, 10'000, gp::step>(1'000'000, acquire_attempts, producers);

#endif

    return EXIT_SUCCESS;
}
