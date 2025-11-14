// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "config.hpp"
#include "queue_test.hpp"
#include <xtxn/mpsc_queue.hpp>

int main(int, char **) {
    config_console();
    config_profiler();
    queue_test<xtxn::mpsc_queue<item_type>>("CLASSIC MPSC QUEUE TEST", mpsc_test_config {});
    return EXIT_SUCCESS;
}
