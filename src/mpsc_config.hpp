// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <cstdint>
#include <algorithm>
#include <thread>
#include "execute_before_main.hpp"

#ifdef _DEBUG
    constexpr int pre_test_iters { 200 };
    constexpr int64_t pre_test_items { 100 };
#else
    constexpr int pre_test_iters { 2'000 };
    constexpr int64_t pre_test_items { 100 };
#endif

inline unsigned workers_a {};
inline unsigned workers_b {};
inline unsigned workers_c {};

EXECUTE_BEFORE_MAIN(perform_config) {
    workers_b = std::thread::hardware_concurrency();
    workers_a = std::max(1u, workers_b - 1);
    workers_c = workers_b << 1;
}
