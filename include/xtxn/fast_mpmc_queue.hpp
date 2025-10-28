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
    enum class growth_policy { call, round, step };
    constexpr int32_t max_attempts [[maybe_unused]] { std::numeric_limits<int32_t>::max() };

    template<std::default_initializable T, int32_t S = 0x10, int32_t L = 0x10'0000, growth_policy G = growth_policy::round>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    class alignas(std::hardware_constructive_interference_size) fast_mpmc_queue final {
    protected:
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
        const int32_t m_acquire_attempts;
        spinlock<> m_spinlock {};
        /*const bool m_auto_complete;*/
        bool m_producing { true };
        bool m_consuming { true };

    public:
        using payload_type [[maybe_unused]] = T;

        static constexpr int32_t c_block_size [[maybe_unused]] { S };
        static constexpr int32_t c_max_capacity [[maybe_unused]] { L };

        explicit fast_mpmc_queue(/*bool = true,*/ int32_t = 5);
        fast_mpmc_queue(const fast_mpmc_queue &) = delete;
        fast_mpmc_queue(fast_mpmc_queue &&) = delete;
        ~fast_mpmc_queue() { delete m_first_block; }

        fast_mpmc_queue & operator=(const fast_mpmc_queue &) = delete;
        fast_mpmc_queue & operator=(fast_mpmc_queue &&) = delete;

        [[nodiscard, maybe_unused]]
        auto capacity() const noexcept {
            return m_capacity.load(std::memory_order_acquire);
        }

        [[nodiscard, maybe_unused]]
        auto free_slots() const noexcept {
            return m_free.load(std::memory_order_acquire);
        }

        [[nodiscard, maybe_unused]] bool empty() const noexcept;
        [[nodiscard, maybe_unused]] bool producing() const noexcept { return m_producing; }
        [[nodiscard, maybe_unused]] bool consuming() const noexcept { return m_consuming; }

        [[nodiscard]] producer_accessor producer_slot(int32_t = 0);
        [[nodiscard]] consumer_accessor consumer_slot();

        [[maybe_unused]] void shutdown() noexcept { m_producing = false; }
        [[maybe_unused]] void stop() noexcept { m_producing = false; m_consuming = false; }

    protected:
        bool grow() noexcept;
    };

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    struct alignas(std::hardware_constructive_interference_size) fast_mpmc_queue<T, S, L, G>::slot final {
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

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    struct fast_mpmc_queue<T, S, L, G>::block final {
        std::array<slot, static_cast<size_t>(S)> m_slots {};
        block * m_next { nullptr };

        block();
        block(const block &) = delete;
        block(block &&) = delete;
        block(block *, block *);
        ~block() { delete m_next; }

        block & operator=(const block &) = delete;
        block & operator=(block &&) = delete;

        [[nodiscard]] slot * first_slot() noexcept { return &m_slots[0]; }
        [[nodiscard]] slot * last_slot() noexcept { return &m_slots[S - 1]; }
    };

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    fast_mpmc_queue<T, S, L, G>::block::block() {
        auto it = m_slots.begin();
        auto last = m_slots.end() - 1;
        while (it != last) {
            auto current = it++;
            current->m_next = &(*it);
        }
        last->m_next = &m_slots[0];
    }

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    fast_mpmc_queue<T, S, L, G>::block::block(block * first_block, block * last_block) {
        assert(first_block);
        assert(last_block);
        auto it = m_slots.begin();
        auto last = m_slots.end() - 1;
        while (it != last) {
            auto current = it++;
            current->m_next = &(*it);
        }
        last->m_next = first_block->first_slot();
        last_block->last_slot()->m_next = &m_slots[0];
        last_block->m_next = this;
    }

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    class alignas(std::hardware_constructive_interference_size) fast_mpmc_queue<T, S, L, G>::base_accessor {
    protected:
        fast_mpmc_queue & m_queue;
        slot * const m_slot;
        /*bool m_complete;*/

    public:
        base_accessor() = delete;
        base_accessor(const base_accessor &) = delete;
        base_accessor(base_accessor &&) = delete;

        base_accessor(fast_mpmc_queue & queue, slot * slot/*, bool complete*/)
        : m_queue { queue }, m_slot { slot }/*, m_complete { complete }*/ {}

        ~base_accessor() = default;

        base_accessor & operator=(const base_accessor &) = delete;
        base_accessor & operator=(base_accessor &&) = delete;
        [[nodiscard, maybe_unused]] T * operator->() noexcept { return &(m_slot->m_payload); }
        [[nodiscard, maybe_unused]] T & operator*() noexcept { return m_slot->m_payload; }

        /*void complete() { m_complete = true; };*/
    };

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    class fast_mpmc_queue<T, S, L, G>::producer_accessor final : public base_accessor {
    protected:
        using base_accessor::m_queue;
        using base_accessor::m_slot;
        /*using base_accessor::m_complete;*/

    public:
        producer_accessor() = delete;
        producer_accessor(const producer_accessor &) = delete;
        producer_accessor(producer_accessor &&) = delete;

        producer_accessor(fast_mpmc_queue & queue, slot * slot/*, bool complete*/)
        : base_accessor { queue, slot/*, complete*/ } {
            if (slot) {
                queue.m_free.fetch_sub(1, std::memory_order_acq_rel);
            }
        }

        ~producer_accessor() {
            if (m_slot) {
                /*if (m_complete) {*/
                    m_slot->m_state.store(state::ready, std::memory_order_release);
                /*} else {
                    m_queue.m_free.fetch_add(1, std::memory_order_acq_rel);
                    m_slot->m_state.store(state::free, std::memory_order_release);
                }*/
            }
        }

        producer_accessor & operator=(const producer_accessor &) = delete;
        producer_accessor & operator=(producer_accessor &&) = delete;

        [[nodiscard, maybe_unused]]
        explicit operator bool() {
            return m_slot && m_slot->m_state.load(std::memory_order_acquire) == state::producer_locked;
        }
    };

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    class fast_mpmc_queue<T, S, L, G>::consumer_accessor final : public base_accessor {
    protected:
        using base_accessor::m_queue;
        using base_accessor::m_slot;
        /*using base_accessor::m_complete;*/

    public:
        consumer_accessor() = delete;
        consumer_accessor(const consumer_accessor &) = delete;
        consumer_accessor(consumer_accessor &&) = delete;

        consumer_accessor(fast_mpmc_queue & queue, slot * slot/*, bool complete*/)
        : base_accessor { queue, slot/*, complete*/ } {}

        ~consumer_accessor() {
            if (m_slot) {
                /*if (m_complete) {*/
                    m_queue.m_free.fetch_add(1, std::memory_order_acq_rel);
                    m_slot->m_state.store(state::free, std::memory_order_release);
                /*} else {
                    m_slot->m_state.store(state::ready, std::memory_order_release);
                }*/
            }
        };

        consumer_accessor & operator=(const consumer_accessor &) = delete;
        consumer_accessor & operator=(consumer_accessor &&) = delete;

        [[nodiscard, maybe_unused]]
        explicit operator bool() {
            return m_slot && m_slot->m_state.load(std::memory_order_acquire) == state::consumer_locked;
        }
    };

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    fast_mpmc_queue<T, S, L, G>::fast_mpmc_queue(/*bool auto_complete,*/ int32_t producer_slot_acquire_attempts)
    :   m_first_block { new block }, m_last_block { m_first_block },
        m_acquire_attempts { producer_slot_acquire_attempts }/*, m_auto_complete { auto_complete }*/ {
        assert(producer_slot_acquire_attempts > 0);
        slot * first_slot { m_first_block->first_slot() };
        m_producer_cursor.store(first_slot, std::memory_order_relaxed);
        m_consumer_cursor.store(first_slot, std::memory_order_relaxed);
    }

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    bool fast_mpmc_queue<T, S, L, G>::empty() const noexcept {
        return m_free.load(std::memory_order_acquire) == m_capacity.load(std::memory_order_acquire);
        // return m_free.load(std::memory_order_seq_cst) == m_capacity.load(std::memory_order_seq_cst);
    }

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    auto fast_mpmc_queue<T, S, L, G>::producer_slot(int32_t slot_acquire_attempts) -> producer_accessor {
        if (!m_free.load(std::memory_order_acquire) && !grow()) {
            return { *this, nullptr/*, false*/ };
        }
        int_fast32_t attempts { slot_acquire_attempts > 0 ? slot_acquire_attempts - 1 : m_acquire_attempts - 1 };
        slot * sentinel {
            m_producer_cursor.exchange(
                m_producer_cursor.load(std::memory_order_acquire)->m_next,
                std::memory_order_acq_rel
            )
        };
        slot * current { sentinel };
        while (m_free.load(std::memory_order_acquire) && m_producing) {
            state slot_state { state::free };
            if (
                current->m_state.compare_exchange_strong(
                    slot_state, state::producer_locked,
                    std::memory_order_acq_rel, std::memory_order_acquire
                )
            ) {
                return { *this, current/*, m_auto_complete*/ };
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
                if constexpr (G == growth_policy::round) {
                    if (!m_free.load(std::memory_order_acquire) && !grow()) {
                        return { *this, nullptr/*, false*/ };
                    }
                }
            }
            if constexpr (G == growth_policy::step) {
                if (!m_free.load(std::memory_order_acquire) && !grow()) {
                    return { *this, nullptr/*, false*/ };
                }
            }
        }
        return { *this, nullptr/*, false*/ };
    }

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    auto fast_mpmc_queue<T, S, L, G>::consumer_slot() -> consumer_accessor {
        if (m_free.load(std::memory_order_acquire) == m_capacity.load(std::memory_order_acquire)) {
            return { *this, nullptr/*, false*/ };
        }
        slot * current {
            m_producer_cursor.exchange(
                m_producer_cursor.load(std::memory_order_acquire)->m_next,
                std::memory_order_acq_rel
            )
        };
        while (m_free.load(std::memory_order_acquire) != m_capacity.load(std::memory_order_acquire) && m_consuming) {
            state slot_state { state::ready };
            if (
                current->m_state.compare_exchange_strong(
                    slot_state, state::consumer_locked,
                    std::memory_order_acq_rel, std::memory_order_acquire
                )
            ) {
                return { *this, current/*, m_auto_complete*/ };
            }
            current = m_consumer_cursor.exchange(
                m_consumer_cursor.load(std::memory_order_acquire)->m_next,
                std::memory_order_acq_rel
            );
        }
        return { *this, nullptr/*, false*/ };
    }

    template<std::default_initializable T, int32_t S, int32_t L, growth_policy G>
    requires (S >= 4) && (L <= std::numeric_limits<int32_t>::max()) && (S <= L)
    bool fast_mpmc_queue<T, S, L, G>::grow() noexcept {
        scoped_lock lock { m_spinlock };
        if (m_capacity.load(std::memory_order_acquire) + S > L) {
            return false;
        }
        block * new_block { new block { m_first_block, m_last_block } };
        m_last_block = new_block;
        m_capacity.fetch_add(S, std::memory_order_acq_rel);
        m_free.fetch_add(S, std::memory_order_acq_rel);
        return true;
    }
}
