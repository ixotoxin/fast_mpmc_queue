// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include "config.hpp"
#include "queue_test.hpp"
#include <xtxn/mpsc_queue.hpp>

int main(int, char **) {
    config_console();
    config_profiler();
    test::perform<xtxn::mpsc_queue<test::item_type>>("CLASSIC MPSC QUEUE TEST", test::config::mpsc {});
    return EXIT_SUCCESS;
}
