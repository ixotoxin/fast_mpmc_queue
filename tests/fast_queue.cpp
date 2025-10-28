// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include <string>
#include <xtxn/fast_mpmc_queue.hpp>
#include <gtest/gtest.h>

using namespace std;
using namespace xtxn;

TEST(lib_fast_mpmc_queue, seq_test) {
    xtxn::fast_mpmc_queue<int, 10, 20> queue {};

    for (int i = 30; i; --i) {
        auto slot = queue.producer_slot();
        if (i > 10) {
            EXPECT_TRUE(static_cast<bool>(slot));
        } else {
            EXPECT_FALSE(static_cast<bool>(slot));
        }
        if (slot) {
            *slot = i;
            /*slot.complete();*/
        }
    }

    for (int i = 30; i > 20; --i) {
        auto slot = queue.consumer_slot();
        EXPECT_TRUE(static_cast<bool>(slot));
        if (slot) {
            EXPECT_TRUE(*slot == i);
            /*slot.complete();*/
        }
    }

    for (int i = 30; i; --i) {
        auto slot = queue.producer_slot();
        if (i > 20) {
            EXPECT_TRUE(static_cast<bool>(slot));
        } else {
            EXPECT_FALSE(static_cast<bool>(slot));
        }
        if (slot) {
            *slot = i;
            /*slot.complete();*/
        }
    }

    for (int i = 20; i > 10; --i) {
        auto slot = queue.consumer_slot();
        EXPECT_TRUE(static_cast<bool>(slot));
        if (slot) {
            EXPECT_TRUE(*slot == i);
            /*slot.complete();*/
        }
    }

    EXPECT_FALSE(queue.empty());

    for (int i = 30; i; --i) {
        auto slot = queue.consumer_slot();
        if (i > 20) {
            EXPECT_TRUE(static_cast<bool>(slot));
        } else {
            EXPECT_FALSE(static_cast<bool>(slot));
        }
        if (slot) {
            EXPECT_TRUE(*slot == i);
            /*slot.complete();*/
        }
    }

    EXPECT_TRUE(queue.empty());
    EXPECT_TRUE(queue.capacity() == 20);
}

TEST(lib_fast_mpmc_queue, queue_of_primitive) {
    xtxn::fast_mpmc_queue<int, 10, 40> queue {};

    for (int i = 50; i; --i) {
        auto slot = queue.producer_slot();
        if (i > 10) {
            EXPECT_TRUE(static_cast<bool>(slot));
        } else {
            EXPECT_FALSE(static_cast<bool>(slot));
        }
        if (slot) {
            *slot = i;
            /*slot.complete();*/
        }
    }

    for (int i = 50; i; --i) {
        auto slot = queue.consumer_slot();
        if (i > 10) {
            EXPECT_TRUE(static_cast<bool>(slot));
        } else {
            EXPECT_FALSE(static_cast<bool>(slot));
        }
        if (slot) {
            EXPECT_TRUE(*slot == i);
            /*slot.complete();*/
        }
    }
}

TEST(lib_fast_mpmc_queue, queue_of_struct) {
    struct payload {
        std::string m_str {};
        int m_int { 0 };
    };

    xtxn::fast_mpmc_queue<payload, 10, 40> queue {};

    for (int i = 50; i; --i) {
        auto slot = queue.producer_slot();
        if (i > 10) {
            EXPECT_TRUE(static_cast<bool>(slot));
        } else {
            EXPECT_FALSE(static_cast<bool>(slot));
        }
        if (slot) {
            slot->m_str.assign("item");
            slot->m_str.append(std::to_string(i));
            slot->m_int = i;
            /*slot.complete();*/
        }
    }

    for (int i = 50; i; --i) {
        auto slot = queue.consumer_slot();
        if (i > 10) {
            EXPECT_TRUE(static_cast<bool>(slot));
        } else {
            EXPECT_FALSE(static_cast<bool>(slot));
        }
        if (slot) {
            std::string str { "item" + std::to_string(i) };
            EXPECT_TRUE(slot->m_str == str);
            EXPECT_TRUE(slot->m_int == i);
            /*slot.complete();*/
        }
    }
}
