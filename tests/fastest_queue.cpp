// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#include <string>
#include <xtxn/fastest_mpmc_queue.hpp>
#include <gtest/gtest.h>

using namespace std;
using namespace xtxn;

TEST(lib_fastest_mpmc_queue, queue_of_primitive) {
    xtxn::fastest_mpmc_queue<int, 40> queue {};

    for (int i = 50; i; --i) {
        auto slot = queue.producer_slot();
        if (i > 10) {
            EXPECT_TRUE(static_cast<bool>(slot));
        } else {
            EXPECT_FALSE(static_cast<bool>(slot));
        }
        if (slot) {
            *slot = i;
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
        }
    }
}

TEST(lib_fastest_mpmc_queue, queue_of_struct) {
    struct payload {
        std::string m_str {};
        int m_int { 0 };

        [[maybe_unused]] void set_bool(bool val) { m_bool = val; }
        [[nodiscard, maybe_unused]] bool get_bool() { return m_bool; } // NOLINT(*-make-member-function-const)
        [[nodiscard, maybe_unused]] bool get_bool() const { return m_bool; }

    private:
        bool m_bool {};
    };

    xtxn::fastest_mpmc_queue<payload, 40> queue {};

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
            (*slot).set_bool(i > 40);
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
            EXPECT_TRUE((*slot).m_str == str);
            EXPECT_TRUE(slot->m_int == i);
            EXPECT_TRUE((*slot).m_int == i);
            if (i > 40) {
                EXPECT_TRUE(slot->get_bool());
            } else {
                EXPECT_FALSE((*slot).get_bool());
            }
        }
    }
}

TEST(lib_fastest_mpmc_queue, order_test) {
    xtxn::fastest_mpmc_queue<int, 20> queue {};

    for (int i = 30; i; --i) {
        auto slot = queue.producer_slot();
        if (i > 10) {
            EXPECT_TRUE(static_cast<bool>(slot));
        } else {
            EXPECT_FALSE(static_cast<bool>(slot));
        }
        if (slot) {
            *slot = i;
        }
    }

    for (int i = 30; i > 20; --i) {
        auto slot = queue.consumer_slot();
        EXPECT_TRUE(static_cast<bool>(slot));
        if (slot) {
            EXPECT_TRUE(*slot == i);
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
        }
    }

    for (int i = 20; i > 10; --i) {
        auto slot = queue.consumer_slot();
        EXPECT_TRUE(static_cast<bool>(slot));
        if (slot) {
            EXPECT_TRUE(*slot == i);
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
        }
    }

    EXPECT_TRUE(queue.empty());
    EXPECT_TRUE(queue.capacity() == 20);
}
