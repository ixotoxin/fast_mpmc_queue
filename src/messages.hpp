// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <cstdint>
#include <concepts>
#include <iomanip>
#include <sstream>
#include <string_view>

constexpr std::string_view thin_separator {
    "  -------------------------------------------------------------\n"
};

constexpr std::string_view thick_separator {
    "=================================================================\n"
};

constexpr std::string_view prelim_test {
    "=================================================================\n"
    "   The preliminary test"
};

constexpr std::string_view has_failed {
    " has failed\n"
};

constexpr std::string_view is_complete {
    " is complete\n"
    "=================================================================\n"
    "   The test with different numbers of items\n"
    "  -------------------------------------------------------------\n"
};

constexpr std::string_view diff_size_and_attempts {
    "   Test with different block sizes and number of attempts\n"
    "   to acquire a slot\n"
    "  -------------------------------------------------------------\n"
};

constexpr std::string_view diff_workers {
    "   Test with different number of workers\n"
    "  -------------------------------------------------------------\n"
};

constexpr std::string_view diff_workers_and_policies {
    "   Test with different number of workers and growth policies\n"
    "  -------------------------------------------------------------\n"
};

constexpr std::string_view all_tests_passed {
    "   ALL TESTS PASSED\n"
    "=================================================================\n"
};

inline void summary_a(
    std::stringstream & stream,
    const int64_t items
) {
    stream
        << "\n   Number of processed items: " << items << '\n';
}

inline void summary_b(
    std::stringstream & stream,
    const int32_t attempts
) {
    stream
        << "   Slot acquire attempts: " << attempts << '\n';
}

inline void summary_b(
    std::stringstream & stream,
    const std::string_view policy,
    const int32_t attempts
) {
    stream
        << "   Queue growth policy: allow at each " << policy << "\n"
           "   Slot acquire attempts: " << attempts << '\n';
}

inline void summary_c(
    std::stringstream & stream,
    const unsigned producers,
    const int64_t pro_time,
    const int64_t pro_successes,
    const int64_t pro_fails,
    const unsigned consumers,
    const int64_t con_time,
    const int64_t con_successes,
    const int64_t con_fails
) {
    stream
        << std::fixed << std::setprecision(2)
        << "  -----------+------+--------------+-------------+-------------\n"
           "   WRK. TYPE | NUM. |  ACQU. TIME  | ACQU. SUCC. | ACQU. FAILS\n"
           "  -----------+------+--------------+-------------+-------------\n"
           "   Producers | "
        << std::setw(4) << producers << " | "
        << std::setw(9) << (static_cast<double>(pro_time) / 1'000) << " ms | "
        << std::setw(11) << pro_successes << " | "
        << std::setw(11) << pro_fails << "\n"
           "   Consumers | "
        << std::setw(4) << consumers << " | "
        << std::setw(9) << (static_cast<double>(con_time) / 1'000)  << " ms | "
        << std::setw(11) << con_successes  << " | "
        << std::setw(11) << con_fails  << "\n"
           "  -----------+------+--------------+-------------+-------------\n";
}

inline void summary_d(
    std::stringstream & stream,
    const std::integral auto capacity,
    const std::integral auto block_size,
    const std::integral auto max_capacity
) {
    stream
        << "   Actual queue capacity: " << capacity
        << " slot (min: " << block_size << ", max: " << max_capacity << ")\n";
}

inline void summary_d(
    std::stringstream & stream,
    const std::integral auto capacity
) {
    stream
        << "   Queue capacity: " << capacity << '\n';
}

inline void summary_e(
    std::stringstream & stream,
    const bool ok,
    const int64_t total_time
) {
    stream
        << "   Control sum: " << (ok ? "OK" : "Invalid") << "\n"
           "   Real total time: " << (static_cast<double>(total_time) / 1'000) << " ms\n\n";
}
