// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <atomic>
#include <vector>
#include <thread>
#include <latch>
#include <xtxn/mpsc_queue.hpp>

#if defined(_WIN32) && defined(_DEBUG)
#   include <crtdbg.h>
#endif

void queue_test(std::stringstream & stream, bool & ok, const int64_t items, const unsigned producers = 5) {
    xtxn::mpsc_queue<int_fast64_t> queue {};
    std::vector<std::jthread> pool {};
    std::latch exit_latch { producers + 2 };
    std::atomic_int_fast64_t pro_time { 0 };
    std::atomic_int_fast64_t pro_successes { 0 };
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
                auto item = queue.dequeue();
                auto t2 = std::chrono::steady_clock::now();
                con_time.fetch_add(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
                if (item) {
                    result.fetch_add(*item, std::memory_order_seq_cst);
                    con_successes.fetch_add(1, std::memory_order_seq_cst);
                } else {
                    con_fails.fetch_add(1, std::memory_order_seq_cst);
                    std::this_thread::yield();
                }
            }
            exit_latch.arrive_and_wait();
        }
    );

    for (unsigned i = producers; i; --i) {
        pool.emplace_back(
            [& exit_latch, & queue, & counter, & pro_time, & pro_successes] {
                int_fast64_t value { counter.fetch_sub(1, std::memory_order_seq_cst) };
                while (value > 0) {
                    auto t1 = std::chrono::steady_clock::now();
                    queue.enqueue(value);
                    auto t2 = std::chrono::steady_clock::now();
                    pro_time.fetch_add(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
                    value = counter.fetch_sub(1, std::memory_order_seq_cst);
                    pro_successes.fetch_add(1, std::memory_order_seq_cst);
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

    stream
        << std::fixed << std::setprecision(2)
        << "\n  -----------+------+--------------+-------------+-------------\n"
           "   WRK. TYPE | NUM. |  ACQU. TIME  | ACQU. SUCC. | ACQU. FAILS\n"
           "  -----------+------+--------------+-------------+-------------\n"
           "   Producers | "
        << std::setw(4) << producers << " | "
        << std::setw(9) << (static_cast<double>(pro_time) / 1'000) << " ms | "
        << std::setw(11) << pro_successes << " |           0\n"
           "   Consumers |    1 | "
        << std::setw(9) << (static_cast<double>(con_time) / 1'000)  << " ms | "
        << std::setw(11) << con_successes  << " | "
        << std::setw(11) << con_fails  << "\n"
           "  -----------+------+--------------+-------------+-------------\n"
           "   Control sum: " << (ok ? "OK" : "Invalid") << "\n"
           "   Real total time: " << (static_cast<double>(t3) / 1'000) << " ms\n\n"
           "=================================================================\n";
}

void queue_test(const int64_t items, const unsigned producers = 5) {
    std::stringstream str {};
    bool ok {};
    queue_test(str, ok, items, producers);
    std::cout << str.str();
}

int main(int, char **) {
#ifdef _DEBUG
#   if defined(_WIN32) && (defined(_MSC_VER) || defined(__clang__))
    {
        constexpr auto report_mode = /*_CRTDBG_MODE_DEBUG |*/ _CRTDBG_MODE_FILE /*| _CRTDBG_MODE_WNDW*/;
        ::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
        ::_CrtSetReportMode(_CRT_ASSERT, report_mode);
        ::_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        ::_CrtSetReportMode(_CRT_WARN, report_mode);
        ::_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        ::_CrtSetReportMode(_CRT_ERROR, report_mode);
        ::_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    }
#   endif
    constexpr int pre_test_iters { 200 };
#else
    constexpr int pre_test_iters { 2'000 };
#endif

    std::cout
        << "=================================================================\n"
           "   CLASSIC MPSC QUEUE TEST  \n"
           "=================================================================\n";

    for (int i = pre_test_iters; i; --i) {
        std::stringstream str {};
        bool ok {};
        queue_test(str, ok, 100ll);
        if (!ok) {
            std::cout << str.str();
            break;
        }
    }

    std::cout
        << "   The preliminary test is completed  \n"
           "=================================================================\n";

    queue_test(100ll);
    queue_test(1'000ll);
    queue_test(10'000ll);

#ifndef _DEBUG

    queue_test(100'000ll);
    queue_test(1'000'000ll);

    unsigned producers = std::thread::hardware_concurrency() - 1;
    queue_test(1'000'000ll, producers);

#endif

    return EXIT_SUCCESS;
}
