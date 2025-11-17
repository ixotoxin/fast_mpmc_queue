// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "config.hpp"
#include "queue_test.hpp"
#include <xtxn/mpmcsl_queue.hpp>

int main(int, char **) {
    config_console();
    config_profiler();
    test::perform<xtxn::mpmcsl_queue<test::item_type>>(
        "CLASSIC MPMC QUEUE TEST (MPSC WITH SPINLOCK)",
        test::config::mpmc {}
    );
    return EXIT_SUCCESS;
}
