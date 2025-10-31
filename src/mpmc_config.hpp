// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

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

inline unsigned producers_a {};
inline unsigned consumers_a {};
inline unsigned producers_b {};
inline unsigned consumers_b {};
inline unsigned producers_c {};
inline unsigned consumers_c {};
inline unsigned producers_d {};
inline unsigned consumers_d {};

EXECUTE_BEFORE_MAIN(perform_config) {
    producers_c = std::thread::hardware_concurrency();
    consumers_c = producers_c;
    producers_a = std::max(1u, producers_c >> 1);
    consumers_a = std::max(1u, producers_c - producers_a);
    producers_b = std::max(1u, static_cast<unsigned>(static_cast<double>(producers_c) / 3.0 * 2.0));
    consumers_b = std::max(1u, producers_c - producers_b);
    // producers_d = producers_c + (producers_c >> 1);
    producers_d = producers_c << 1;
    consumers_d = producers_d;
}
