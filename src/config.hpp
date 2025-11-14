// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <cassert>
#include <cstdint>
#include <thread>
#include <algorithm>
#include <utility>

#ifndef _DEBUG
#   undef ENABLE_MEMORY_PROFILING
#endif

#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       error Requires WIN32_LEAN_AND_MEAN definition
#   endif
#   ifndef NOMINMAX
#       error Requires NOMINMAX definition
#   endif
#   include <windows.h>
#   include <io.h>
#   include <fcntl.h>
#   include <clocale>
#   if defined(ENABLE_MEMORY_PROFILING) && defined(_DEBUG) && (defined(_MSC_VER) || defined(__clang__))
#       include <iostream>
#       include <crtdbg.h>
#   else
#       undef ENABLE_MEMORY_PROFILING
#   endif
#else
#   undef ENABLE_MEMORY_PROFILING
#endif

inline void config_console() {
#ifdef _WIN32
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);
    ::setlocale(LC_ALL, ".UTF8");
#endif
}

inline void config_profiler() {
#ifdef ENABLE_MEMORY_PROFILING
    std::cout << "!! Enabled memory profiling !!" << std::endl;
    constexpr auto report_mode = /*_CRTDBG_MODE_DEBUG |*/ _CRTDBG_MODE_FILE /*| _CRTDBG_MODE_WNDW*/;
    ::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
    ::_CrtSetReportMode(_CRT_ASSERT, report_mode);
    ::_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    ::_CrtSetReportMode(_CRT_WARN, report_mode);
    ::_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    ::_CrtSetReportMode(_CRT_ERROR, report_mode);
    ::_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
}

struct prelim_test_config {
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

using test_config = std::pair<unsigned, unsigned>; /** .first => producers number, .second => consumers number **/

struct mpsc_test_config : public prelim_test_config {
    const unsigned cores;
    const test_config set_a;
    const test_config set_b;
    const test_config set_c;
    const test_config set_d;

    mpsc_test_config()
    : prelim_test_config {},
      cores { std::thread::hardware_concurrency() },
      set_a { 1, 1 },
      set_b { std::max(1u, cores - 1), 1 },
      set_c { cores, 1 },
      set_d { cores << 1, 1 } {}

    mpsc_test_config(const mpsc_test_config &) = delete;
    mpsc_test_config(mpsc_test_config &&) = delete;
    ~mpsc_test_config() = default;

    mpsc_test_config & operator=(const mpsc_test_config &) = delete;
    mpsc_test_config & operator=(mpsc_test_config &&) = delete;
};

struct mpmc_test_config : public prelim_test_config {
    const unsigned cores;
    const test_config set_a;
    const test_config set_b;
    const test_config set_c;
    const test_config set_d;

    mpmc_test_config()
    : prelim_test_config {},
      cores { std::thread::hardware_concurrency() },
      set_a { proportion(cores, cores >> 1) },
      set_b { proportion(cores, static_cast<unsigned>(static_cast<double>(cores) / 3.0 * 2.0)) },
      set_c { same(cores) },
      set_d { same(cores << 1) } {}

    mpmc_test_config(const mpmc_test_config &) = delete;
    mpmc_test_config(mpmc_test_config &&) = delete;
    ~mpmc_test_config() = default;

    mpmc_test_config & operator=(const mpmc_test_config &) = delete;
    mpmc_test_config & operator=(mpmc_test_config &&) = delete;

    static test_config same(unsigned workers) {
        return { std::max(1u, workers), std::max(1u, workers) };
    }

    static test_config proportion(unsigned total, unsigned producers) {
        assert(producers <= total);
        return { std::max(1u, producers), std::max(1u, total - std::max(1u, producers)) };
    }
};
