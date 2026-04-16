// Copyright (c) 2025-2026 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "init.hpp"
#include "config.hpp"
#include "dfmpmcq_test.hpp"

int main(int, char **) {
    init::console();
    init::profiler();
    test::perform("DYNAMIC FAST LOCK-FREE MPSC QUEUE TEST", test::config::mpsc {});
    return EXIT_SUCCESS;
}
