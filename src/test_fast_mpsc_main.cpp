// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "config.hpp"
#include "fast_queue_test.hpp"

int main(int, char **) {
    config_console();
    config_profiler();
    test::perform("FAST LOCK-FREE MPSC QUEUE TEST", test::config::mpsc {});
    return EXIT_SUCCESS;
}
