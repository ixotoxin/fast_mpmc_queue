// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <cassert>
#include <concepts>
#include <cstdint>
#include <limits>
#include <array>
#include "spinlock.hpp"

namespace xtxn {
    enum class queue_growth_policy { call, round, step };

    constexpr int32_t queue_default_block_size [[maybe_unused]] { 0x10 };
    constexpr int32_t queue_default_capacity_limit [[maybe_unused]] { queue_default_block_size * 0x1'0000 };
    constexpr int32_t queue_max_capacity_limit [[maybe_unused]] { std::numeric_limits<int32_t>::max() };
    constexpr int32_t queue_default_attempts [[maybe_unused]] { 5 };
    constexpr int32_t queue_max_attempts [[maybe_unused]] { std::numeric_limits<int32_t>::max() };

    template<
        std::default_initializable T,
        bool C = true,
        int32_t S = queue_default_block_size,
        int32_t L = queue_default_capacity_limit,
        int32_t A = queue_default_attempts,
        queue_growth_policy G = queue_growth_policy::round
    >
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    class alignas(std::hardware_constructive_interference_size) fast_mpmc_queue {
        enum class state { free, producer_locked, ready, consumer_locked };

        struct slot;
        struct block;
        class base_accessor;
        class producer_accessor;
        class consumer_accessor;

        block * m_first_block;
        block * m_last_block;
        std::atomic<slot *> m_producer_cursor { nullptr };
        std::atomic<slot *> m_consumer_cursor { nullptr };
        std::atomic_int_fast32_t m_capacity { S };
        std::atomic_int_fast32_t m_free { S };
        std::atomic_bool m_producing { true };
        std::atomic_bool m_consuming { true };
        spinlock<spin::yield_thread> m_spinlock {};

    public:
        using payload_type [[maybe_unused]] = T;

        static constexpr bool c_auto_complete [[maybe_unused]] { C };
        static constexpr int32_t c_block_size [[maybe_unused]] { S };
        static constexpr int32_t c_max_capacity [[maybe_unused]] { L };
        static constexpr int32_t c_default_attempts [[maybe_unused]] { A };

        explicit fast_mpmc_queue();
        fast_mpmc_queue(const fast_mpmc_queue &) = delete;
        fast_mpmc_queue(fast_mpmc_queue &&) = delete;
        ~fast_mpmc_queue() { delete m_first_block; }

        fast_mpmc_queue & operator=(const fast_mpmc_queue &) = delete;
        fast_mpmc_queue & operator=(fast_mpmc_queue &&) = delete;

        [[nodiscard, maybe_unused]]
        auto capacity() const noexcept {
            return m_capacity.load(std::memory_order_relaxed);
        }

        [[nodiscard, maybe_unused]]
        auto free_slots() const noexcept {
            return m_free.load(std::memory_order_relaxed);
        }

        [[nodiscard, maybe_unused]] bool empty() const noexcept;

        [[nodiscard, maybe_unused]]
        bool producing() const noexcept {
            return m_producing.load(std::memory_order_relaxed);
        }

        [[nodiscard, maybe_unused]]
        bool consuming() const noexcept {
            return m_consuming.load(std::memory_order_relaxed);
        }

        [[nodiscard]] producer_accessor producer_slot(int32_t = c_default_attempts);
        [[nodiscard]] consumer_accessor consumer_slot();

        [[maybe_unused]]
        void shutdown() noexcept {
            m_producing.store(false, std::memory_order_relaxed);
        }

        [[maybe_unused]]
        void stop() noexcept {
            m_producing.store(false, std::memory_order_relaxed);
            m_consuming.store(false, std::memory_order_relaxed);
        }

    protected:
        bool grow() noexcept;
    };

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    struct alignas(std::hardware_constructive_interference_size) fast_mpmc_queue<T, C, S, L, A, G>::slot {
        slot * m_next { nullptr };
        std::atomic<state> m_state { state::free };
        T m_payload {};

        slot() = default;
        slot(const slot &) = delete;
        slot(slot &&) = delete;
        ~slot() = default;

        slot & operator=(const slot &) = delete;
        slot & operator=(slot &&) = delete;
    };

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    struct fast_mpmc_queue<T, C, S, L, A, G>::block {
        std::array<slot, static_cast<size_t>(S)> m_slots {};
        block * m_next { nullptr };

        block();
        block(const block &) = delete;
        block(block &&) = delete;
        explicit block(block * &);
        ~block() { delete m_next; }

        block & operator=(const block &) = delete;
        block & operator=(block &&) = delete;

        [[nodiscard]] slot * first_slot() noexcept { return &m_slots[0]; }
        [[nodiscard]] slot * last_slot() noexcept { return &m_slots[S - 1]; }
    };

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    fast_mpmc_queue<T, C, S, L, A, G>::block::block() {
        auto it = m_slots.begin();
        auto last = m_slots.end() - 1;
        while (it != last) {
            auto current = it++;
            current->m_next = &(*it);
        }
        last->m_next = &m_slots[0];
    }

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    fast_mpmc_queue<T, C, S, L, A, G>::block::block(block * & last_block) {
        assert(last_block);
        auto it = m_slots.begin();
        auto last = m_slots.end() - 1;
        while (it != last) {
            auto current = it++;
            current->m_next = &(*it);
        }
        auto last_slot = last_block->last_slot();
        last->m_next = last_slot->m_next;
        last_slot->m_next = &m_slots[0];
        last_block->m_next = this;
    }

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    class alignas(std::hardware_constructive_interference_size) fast_mpmc_queue<T, C, S, L, A, G>::base_accessor {
    protected:
        fast_mpmc_queue & m_queue;
        slot * const m_slot;
        bool m_complete;

    public:
        base_accessor() = delete;
        base_accessor(const base_accessor &) = delete;
        base_accessor(base_accessor &&) = delete;

        base_accessor(fast_mpmc_queue & queue, slot * slot)
        : m_queue { queue }, m_slot { slot }, m_complete { c_auto_complete } {}

        ~base_accessor() = default;

        base_accessor & operator=(const base_accessor &) = delete;
        base_accessor & operator=(base_accessor &&) = delete;
        [[nodiscard, maybe_unused]] T * operator->() noexcept { return &(m_slot->m_payload); }
        [[nodiscard, maybe_unused]] T & operator*() noexcept { return m_slot->m_payload; }

        [[maybe_unused]] void complete() { m_complete = true; };
    };

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    class fast_mpmc_queue<T, C, S, L, A, G>::producer_accessor : public base_accessor {
    protected:
        using base_accessor::m_queue;
        using base_accessor::m_slot;
        using base_accessor::m_complete;

    public:
        producer_accessor() = delete;
        producer_accessor(const producer_accessor &) = delete;
        producer_accessor(producer_accessor &&) = delete;

        producer_accessor(fast_mpmc_queue & queue, slot * slot)
        : base_accessor { queue, slot } {
            if (slot) {
                queue.m_free.fetch_sub(1, std::memory_order_acq_rel);
            }
        }

        ~producer_accessor() {
            if (m_slot) {
                if (m_complete) {
                    m_slot->m_state.store(state::ready, std::memory_order_release);
                } else {
                    m_queue.m_free.fetch_add(1, std::memory_order_acq_rel);
                    m_slot->m_state.store(state::free, std::memory_order_release);
                }
            }
        }

        producer_accessor & operator=(const producer_accessor &) = delete;
        producer_accessor & operator=(producer_accessor &&) = delete;

        [[nodiscard, maybe_unused]]
        explicit operator bool() {
            return m_slot && m_slot->m_state.load(std::memory_order_acquire) == state::producer_locked;
        }
    };

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    class fast_mpmc_queue<T, C, S, L, A, G>::consumer_accessor : public base_accessor {
    protected:
        using base_accessor::m_queue;
        using base_accessor::m_slot;
        using base_accessor::m_complete;

    public:
        consumer_accessor() = delete;
        consumer_accessor(const consumer_accessor &) = delete;
        consumer_accessor(consumer_accessor &&) = delete;

        consumer_accessor(fast_mpmc_queue & queue, slot * slot)
        : base_accessor { queue, slot } {}

        ~consumer_accessor() {
            if (m_slot) {
                if (m_complete) {
                    m_queue.m_free.fetch_add(1, std::memory_order_acq_rel);
                    m_slot->m_state.store(state::free, std::memory_order_release);
                } else {
                    m_slot->m_state.store(state::ready, std::memory_order_release);
                }
            }
        };

        consumer_accessor & operator=(const consumer_accessor &) = delete;
        consumer_accessor & operator=(consumer_accessor &&) = delete;

        [[nodiscard, maybe_unused]]
        explicit operator bool() {
            return m_slot && m_slot->m_state.load(std::memory_order_acquire) == state::consumer_locked;
        }
    };

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    fast_mpmc_queue<T, C, S, L, A, G>::fast_mpmc_queue()
    :   m_first_block { new block }, m_last_block { m_first_block } {
        slot * first_slot { m_first_block->first_slot() };
        m_producer_cursor.store(first_slot, std::memory_order_relaxed);
        m_consumer_cursor.store(first_slot, std::memory_order_relaxed);
    }

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    bool fast_mpmc_queue<T, C, S, L, A, G>::empty() const noexcept {
        return m_free.load(std::memory_order_acquire) == m_capacity.load(std::memory_order_acquire);
        // return m_free.load(std::memory_order_seq_cst) == m_capacity.load(std::memory_order_seq_cst);
    }

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    auto fast_mpmc_queue<T, C, S, L, A, G>::producer_slot(int32_t slot_acquire_attempts) -> producer_accessor {
        if (!m_free.load(std::memory_order_acquire) && !grow()) {
            return { *this, nullptr };
        }
        int_fast32_t attempts { slot_acquire_attempts - 1 };
        slot * sentinel {
            m_producer_cursor.exchange(
                m_producer_cursor.load(std::memory_order_acquire)->m_next,
                std::memory_order_acq_rel
            )
        };
        slot * current { sentinel };
        while (m_free.load(std::memory_order_acquire) && m_producing.load(std::memory_order_relaxed)) {
            state slot_state { state::free };
            if (
                current->m_state.compare_exchange_strong(
                    slot_state, state::producer_locked,
                    std::memory_order_acq_rel, std::memory_order_acquire
                )
            ) {
                return { *this, current };
            }
            current = m_producer_cursor.exchange(
                m_producer_cursor.load(std::memory_order_acquire)->m_next,
                std::memory_order_acq_rel
            );
            if (current == sentinel) {
                if (attempts == 0) {
                    break;
                }
                --attempts;
                if constexpr (G == queue_growth_policy::round) {
                    if (!m_free.load(std::memory_order_acquire) && !grow()) {
                        return { *this, nullptr };
                    }
                }
            }
            if constexpr (G == queue_growth_policy::step) {
                if (!m_free.load(std::memory_order_acquire) && !grow()) {
                    return { *this, nullptr };
                }
            }
        }
        return { *this, nullptr };
    }

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    auto fast_mpmc_queue<T, C, S, L, A, G>::consumer_slot() -> consumer_accessor {
        if (m_free.load(std::memory_order_acquire) == m_capacity.load(std::memory_order_acquire)) {
            return { *this, nullptr };
        }
        slot * current {
            m_producer_cursor.exchange(
                m_producer_cursor.load(std::memory_order_acquire)->m_next,
                std::memory_order_acq_rel
            )
        };
        while (
            m_free.load(std::memory_order_acquire) != m_capacity.load(std::memory_order_acquire)
            && m_consuming.load(std::memory_order_relaxed)
        ) {
            state slot_state { state::ready };
            if (
                current->m_state.compare_exchange_strong(
                    slot_state, state::consumer_locked,
                    std::memory_order_acq_rel, std::memory_order_acquire
                )
            ) {
                return { *this, current };
            }
            current = m_consumer_cursor.exchange(
                m_consumer_cursor.load(std::memory_order_acquire)->m_next,
                std::memory_order_acq_rel
            );
        }
        return { *this, nullptr };
    }

    template<std::default_initializable T, bool C, int32_t S, int32_t L, int32_t A, queue_growth_policy G>
    requires (S >= 4) && (L <= queue_max_capacity_limit) && (S <= L) && (A > 0) && (A <= queue_max_attempts)
    bool fast_mpmc_queue<T, C, S, L, A, G>::grow() noexcept {
        scoped_lock lock { m_spinlock };
        if (m_capacity.load(std::memory_order_acquire) + S > L) {
            return false;
        }
        m_last_block = new block { m_last_block };
        m_capacity.fetch_add(S, std::memory_order_acq_rel);
        m_free.fetch_add(S, std::memory_order_acq_rel);
        return true;
    }
}
