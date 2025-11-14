// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "config.hpp"
#include "queue_test.hpp"
#include <xtxn/mpmcsl_queue.hpp>

int main(int, char **) {
    config_console();
    config_profiler();
    queue_test<xtxn::mpmcsl_queue<item_type>>("CLASSIC MPMC QUEUE TEST (MPSC WITH SPINLOCK)", mpmc_test_config {});
    return EXIT_SUCCESS;
}
