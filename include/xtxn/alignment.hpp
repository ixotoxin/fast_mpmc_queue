// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <new>

namespace xtxn {
#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t queue_alignment { std::hardware_constructive_interference_size };
#else
    constexpr std::size_t queue_alignment { 64 };
#endif
}
