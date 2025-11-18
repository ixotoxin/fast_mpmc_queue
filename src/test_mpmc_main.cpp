// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "init.hpp"
#include "config.hpp"
#include "queue_test.hpp"
#include <xtxn/mpmc_queue.hpp>

int main(int, char **) {
    init::console();
    init::profiler();
    test::perform<xtxn::mpmc_queue<test::item_type>>(
        "CLASSIC MPMC QUEUE TEST (EPOCH-BASED RECLAMATION)",
        test::config::mpmc {}
    );
    return EXIT_SUCCESS;
}
