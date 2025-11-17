// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <cstdint>
#include <utility>

namespace test {
    using item_type = int_fast64_t;
    using config_set = std::pair<unsigned, unsigned>; /** .first => producers number, .second => consumers number **/
}
