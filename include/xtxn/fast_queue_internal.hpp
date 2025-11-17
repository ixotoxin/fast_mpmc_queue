// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

namespace xtxn {
    constexpr int32_t queue_default_block_size [[maybe_unused]] { 0x10 };
    constexpr int32_t queue_default_capacity_limit [[maybe_unused]] { queue_default_block_size * 0x1'0000 };
    constexpr int32_t queue_max_capacity_limit [[maybe_unused]] { std::numeric_limits<int32_t>::max() };
    constexpr bool queue_default_completion [[maybe_unused]] { true };
    constexpr int32_t queue_default_attempts [[maybe_unused]] { 5 };
    constexpr int32_t queue_max_attempts [[maybe_unused]] { std::numeric_limits<int32_t>::max() };

    enum class queue_slot_state { free, prod_locked, ready, cons_locked };

    namespace {
        class auto_completion {
        public:
            static constexpr bool c_auto_complete [[maybe_unused]] { true };

            auto_completion() noexcept = default;
            auto_completion(const auto_completion &) = delete;
            auto_completion(auto_completion &&) = delete;
            ~auto_completion() noexcept = default;

            auto_completion & operator=(const auto_completion &) = delete;
            auto_completion & operator=(auto_completion &&) = delete;

            [[maybe_unused]]
            void complete() noexcept {}
        };

        class manual_completion {
        protected:
            bool m_complete { false };

        public:
            static constexpr bool c_auto_complete [[maybe_unused]] { false };

            manual_completion() noexcept = default;
            manual_completion(const manual_completion &) = delete;
            manual_completion(manual_completion &&) = delete;
            ~manual_completion() = default;

            manual_completion & operator=(const manual_completion &) = delete;
            manual_completion & operator=(manual_completion &&) = delete;

            [[maybe_unused]]
            void complete() noexcept {
                m_complete = true;
            }
        };
    }

    template<bool C = queue_default_completion>
    using queue_slot_completion = std::conditional_t<C, auto_completion, manual_completion>;
}
