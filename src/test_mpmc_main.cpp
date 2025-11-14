// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "config.hpp"
#include "queue_test.hpp"
#include <xtxn/mpmc_queue.hpp>

int main(int, char **) {
    config_console();
    config_profiler();
    queue_test<xtxn::mpmc_queue<item_type>>("CLASSIC MPMC QUEUE TEST (EPOCH-BASED RECLAMATION)", mpmc_test_config {});
    return EXIT_SUCCESS;
}
