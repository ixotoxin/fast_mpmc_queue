// Copyright (c) 2025-2026 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <cassert>
#include <concepts>
#include <mutex>
#include "types.hpp"
#include "fast_mpmc_queue_commons.hpp"
#include "spinlock.hpp"

namespace xtxn {
    enum class queue_growth_policy { call, round, step };

    template<
        std::default_initializable T,
        signed S = queue_default_block_size,
        signed L = queue_default_max_blocks,
        bool C = queue_default_auto_completion,
        unsigned A = queue_default_attempts,
        queue_growth_policy G = queue_growth_policy::round
    >
    requires (S > 1) && (L > 0) && (A > 0)
    class alignas(true_sharing_align) dynamic_fast_mpmc_queue {
        struct slot;
        struct block;
        using slot_completion = queue_slot_completion<C>;
        class producer_accessor;
        class consumer_accessor;
        using mo = std::memory_order;
        using state = queue_slot_state;

        static constexpr bool c_ntdct = std::is_nothrow_default_constructible_v<T>;

        block * m_first_block;
        block * m_last_block;
        std::atomic_int_fast32_t m_capacity { 0 };
        spinlock<> m_spinlock {};
        struct alignas(false_sharing_align) {
            std::atomic<slot *> m_cursor { nullptr };
            std::atomic_flag m_enable {};
        } m_producer;
        struct alignas(false_sharing_align) {
            std::atomic<slot *> m_cursor { nullptr };
            std::atomic_flag m_enable {};
        } m_consumer;
        alignas(false_sharing_align) std::atomic_int_fast32_t m_free { 0 };

        bool grow() noexcept;

    public:
        using payload_type [[maybe_unused]] = T;
        using size_type = decltype(m_capacity)::value_type;

        static constexpr size_type c_block_size [[maybe_unused]] { S };
        static constexpr size_type c_max_capacity [[maybe_unused]] { L };
        static constexpr bool c_auto_complete [[maybe_unused]] { C };
        static constexpr unsigned c_default_attempts [[maybe_unused]] { A };
        static constexpr queue_growth_policy c_growth_policy [[maybe_unused]] { G };

        dynamic_fast_mpmc_queue();
        dynamic_fast_mpmc_queue(const dynamic_fast_mpmc_queue &) = delete;
        dynamic_fast_mpmc_queue(dynamic_fast_mpmc_queue &&) = delete;
        ~dynamic_fast_mpmc_queue();

        dynamic_fast_mpmc_queue & operator=(const dynamic_fast_mpmc_queue &) = delete;
        dynamic_fast_mpmc_queue & operator=(dynamic_fast_mpmc_queue &&) = delete;

        [[nodiscard, maybe_unused]]
        size_type capacity() const noexcept {
            return m_capacity.load(mo::relaxed);
        }

        [[nodiscard, maybe_unused]]
        size_type free_slots() const noexcept {
            return m_free.load(mo::relaxed);
        }

        [[nodiscard, maybe_unused]]
        bool empty() const noexcept {
            return m_free.load(mo::acquire) == m_capacity.load(mo::acquire);
        }

        [[nodiscard, maybe_unused]]
        bool producing() const noexcept {
            return m_producer.m_enable.test(mo::acquire);
        }

        [[nodiscard, maybe_unused]]
        bool consuming() const noexcept {
            return m_consumer.m_enable.test(mo::acquire);
        }

        [[nodiscard]] producer_accessor producer_slot(unsigned = c_default_attempts) noexcept;
        [[nodiscard]] consumer_accessor consumer_slot() noexcept;

        [[maybe_unused]]
        void shutdown() noexcept {
            m_producer.m_enable.clear(mo::release);
        }

        [[maybe_unused]]
        void stop() noexcept {
            m_producer.m_enable.clear(mo::release);
            m_consumer.m_enable.clear(mo::release);
        }
    };

    template<std::default_initializable T, signed S, signed L, bool C, unsigned A, queue_growth_policy G>
    requires (S > 1) && (L > 0) && (A > 0)
    struct dynamic_fast_mpmc_queue<T, S, L, C, A, G>::slot {
        slot * m_next { nullptr };
        alignas(false_sharing_align) std::atomic<state> m_state { state::free };
        alignas(false_sharing_align) T m_payload {};

        slot() noexcept(c_ntdct) = default;
        slot(const slot &) = delete;
        slot(slot &&) = delete;
        ~slot() = default;

        slot & operator=(const slot &) = delete;
        slot & operator=(slot &&) = delete;
    };

    template<std::default_initializable T, signed S, signed L, bool C, unsigned A, queue_growth_policy G>
    requires (S > 1) && (L > 0) && (A > 0)
    struct dynamic_fast_mpmc_queue<T, S, L, C, A, G>::block {
        slot m_slots[static_cast<size_t>(S)] {};
        block * m_next { nullptr };

        block() noexcept(c_ntdct) = default;
        block(const block &) = delete;
        ~block() { delete m_next; }

        block & operator=(const block &) = delete;
        block & operator=(block &&) = delete;

        [[nodiscard]]
        slot * first_slot() noexcept {
            return &m_slots[0];
        }

        [[nodiscard]]
        slot * last_slot() noexcept {
            return &m_slots[S - 1];
        }

        slot * assemble(slot * next = nullptr) noexcept {
            auto first = &m_slots[0];
            auto last = &m_slots[S - 1];
            auto it = first;
            while (it != last) {
                auto current = it++;
                current->m_next = it;
            }
            last->m_next = next ? next : first;
            return first;
        }
    };

    template<std::default_initializable T, signed S, signed L, bool C, unsigned A, queue_growth_policy G>
    requires (S > 1) && (L > 0) && (A > 0)
    class dynamic_fast_mpmc_queue<T, S, L, C, A, G>::producer_accessor : public slot_completion {
    protected:
        dynamic_fast_mpmc_queue * const m_queue { nullptr };
        slot * const m_slot { nullptr };

    public:
        producer_accessor() noexcept = default;
        producer_accessor(const producer_accessor &) = delete;
        producer_accessor(producer_accessor &&) = delete;

        producer_accessor(dynamic_fast_mpmc_queue * queue, slot * slot) noexcept
        : slot_completion {}, m_queue { queue }, m_slot { slot } {
            assert(m_queue);
            assert(m_slot);
            assert(m_slot->m_state.load(mo::acquire) == state::prod_locked);
            m_queue->m_free.fetch_sub(1, mo::acq_rel);
        }

        ~producer_accessor() override;

        producer_accessor & operator=(const producer_accessor &) = delete;
        producer_accessor & operator=(producer_accessor &&) = delete;

        [[nodiscard, maybe_unused]]
        T * operator->() noexcept {
            assert(m_queue);
            assert(m_slot);
            assert(m_slot->m_state.load(mo::acquire) == state::prod_locked);
            return &m_slot->m_payload;
        }

        [[nodiscard, maybe_unused]]
        T & operator*() noexcept {
            assert(m_queue);
            assert(m_slot);
            assert(m_slot->m_state.load(mo::acquire) == state::prod_locked);
            return m_slot->m_payload;
        }

        [[nodiscard, maybe_unused]]
        explicit operator bool() noexcept {
            assert(
                (!m_queue && !m_slot)
                || (m_queue && m_slot && m_slot->m_state.load(mo::acquire) == state::prod_locked)
            );
            return m_slot;
        }
    };

    template<std::default_initializable T, signed S, signed L, bool C, unsigned A, queue_growth_policy G>
    requires (S > 1) && (L > 0) && (A > 0)
    dynamic_fast_mpmc_queue<T, S, L, C, A, G>::producer_accessor::~producer_accessor() {
        if (m_slot) {
            if constexpr (slot_completion::c_auto_complete) {
                m_slot->m_state.store(state::ready, mo::release);
            } else {
                if (slot_completion::m_complete) {
                    m_slot->m_state.store(state::ready, mo::release);
                } else {
                    m_slot->m_state.store(state::free, mo::release);
                    m_queue->m_free.fetch_add(1, mo::acq_rel);
                }
            }
        }
    }

    template<std::default_initializable T, signed S, signed L, bool C, unsigned A, queue_growth_policy G>
    requires (S > 1) && (L > 0) && (A > 0)
    class dynamic_fast_mpmc_queue<T, S, L, C, A, G>::consumer_accessor : public slot_completion {
    protected:
        dynamic_fast_mpmc_queue * const m_queue { nullptr };
        slot * const m_slot { nullptr };

    public:
        consumer_accessor() noexcept = default;
        consumer_accessor(const consumer_accessor &) = delete;
        consumer_accessor(consumer_accessor &&) = delete;

        consumer_accessor(dynamic_fast_mpmc_queue * queue, slot * slot) noexcept
        : slot_completion {}, m_queue { queue }, m_slot { slot } {
            assert(m_queue);
            assert(m_slot);
            assert(m_slot->m_state.load(mo::acquire) == state::cons_locked);
        }

        ~consumer_accessor() override;

        consumer_accessor & operator=(const consumer_accessor &) = delete;
        consumer_accessor & operator=(consumer_accessor &&) = delete;

        [[nodiscard, maybe_unused]]
        const T * operator->() noexcept {
            assert(m_queue);
            assert(m_slot);
            assert(m_slot->m_state.load(mo::acquire) == state::cons_locked);
            return &m_slot->m_payload;
        }

        [[nodiscard, maybe_unused]]
        const T & operator*() noexcept {
            assert(m_queue);
            assert(m_slot);
            assert(m_slot->m_state.load(mo::acquire) == state::cons_locked);
            return m_slot->m_payload;
        }

        [[nodiscard, maybe_unused]]
        explicit operator bool() noexcept {
            assert(
                (!m_queue && !m_slot)
                || (m_queue && m_slot && m_slot->m_state.load(mo::acquire) == state::cons_locked)
            );
            return m_slot;
        }
    };

    template<std::default_initializable T, signed S, signed L, bool C, unsigned A, queue_growth_policy G>
    requires (S > 1) && (L > 0) && (A > 0)
    dynamic_fast_mpmc_queue<T, S, L, C, A, G>::consumer_accessor::~consumer_accessor() {
        if (m_slot) {
            if constexpr (slot_completion::c_auto_complete) {
                m_slot->m_state.store(state::free, mo::release);
                m_queue->m_free.fetch_add(1, mo::acq_rel);
            } else {
                if (slot_completion::m_complete) {
                    m_slot->m_state.store(state::free, mo::release);
                    m_queue->m_free.fetch_add(1, mo::acq_rel);
                } else {
                    m_slot->m_state.store(state::ready, mo::release);
                }
            }
        }
    }

    template<std::default_initializable T, signed S, signed L, bool C, unsigned A, queue_growth_policy G>
    requires (S > 1) && (L > 0) && (A > 0)
    dynamic_fast_mpmc_queue<T, S, L, C, A, G>::dynamic_fast_mpmc_queue()
    :   m_first_block { new block }, m_last_block { m_first_block } {
        slot * first_slot { m_first_block->assemble() };
        m_producer.m_cursor.store(first_slot, mo::relaxed);
        m_consumer.m_cursor.store(first_slot, mo::relaxed);
        m_capacity.store(S, mo::release);
        m_free.store(S, mo::release);
        m_producer.m_enable.test_and_set(mo::acquire);
        m_consumer.m_enable.test_and_set(mo::acquire);
    }

    template<std::default_initializable T, signed S, signed L, bool C, unsigned A, queue_growth_policy G>
    requires (S > 1) && (L > 0) && (A > 0)
    dynamic_fast_mpmc_queue<T, S, L, C, A, G>::~dynamic_fast_mpmc_queue() {
        delete m_first_block;
    }

    template<std::default_initializable T, signed S, signed L, bool C, unsigned A, queue_growth_policy G>
    requires (S > 1) && (L > 0) && (A > 0)
    auto
    dynamic_fast_mpmc_queue<T, S, L, C, A, G>::producer_slot(unsigned acquire_attempts)
    noexcept -> producer_accessor {
        if (!m_producer.m_enable.test(mo::acquire)) {
            return {};
        }
        if (m_free.load(mo::acquire) == 0 && (m_capacity.load(mo::acquire) >= S * L || !grow())) {
            return {};
        }

        // v3 {{{
        // auto current = m_producerCursor.load(MemOrd::acquire);
        // }}} v3
        for (;;) {
            for (auto count = m_capacity.load(mo::acquire); count; --count) {
                auto state = state::free;
        // v1 {{{
                // auto current = m_producer.m_cursor.exchange(m_producer.m_cursor.load(mo::acquire)->m_next, mo::acq_rel);
        // }}} v1
        // v2 {{{
                auto current = m_producer.m_cursor.load(mo::acquire);
                m_producer.m_cursor.store(current->m_next, mo::release);
        // }}} v2
        // v3 {{{
                // m_producer.m_cursor.compare_exchange_strong(current, current->m_next, mo::acq_rel, mo::acquire);
        // }}} v3
                if (current->m_state.compare_exchange_strong(state, state::prod_locked, mo::acq_rel, mo::acquire)) {
                    return { this, current };
                }
                if (!m_producer.m_enable.test(mo::acquire)) {
                    return {};
                }
                if constexpr (G == queue_growth_policy::step) {
                    if (m_free.load(mo::acquire) == 0 && (m_capacity.load(mo::acquire) >= S * L || !grow())) {
                        return {};
                    }
                }
            }
            if (!--acquire_attempts) {
                return {};
            }
            if constexpr (G == queue_growth_policy::round) {
                if (m_free.load(mo::acquire) == 0 && (m_capacity.load(mo::acquire) >= S * L || !grow())) {
                    return {};
                }
            }
        }
    }

    template<std::default_initializable T, signed S, signed L, bool C, unsigned A, queue_growth_policy G>
    requires (S > 1) && (L > 0) && (A > 0)
    auto dynamic_fast_mpmc_queue<T, S, L, C, A, G>::consumer_slot() noexcept -> consumer_accessor {
        // v3 {{{
        // auto current = m_consumerCursor.load(MemOrd::acquire);
        // }}} v3
        while (m_consumer.m_enable.test(mo::acquire) && m_free.load(mo::acquire) < m_capacity.load(mo::acquire)) {
            auto state = state::ready;
        // v1 {{{
            // auto current = m_consumer.m_cursor.exchange(m_consumer.m_cursor.load(mo::acquire)->m_next, mo::acq_rel);
        // }}} v1
        // v2 {{{
            auto current = m_consumer.m_cursor.load(mo::acquire);
            m_consumer.m_cursor.store(current->m_next, mo::release);
        // }}} v2
        // v3 {{{
            // m_consumer.m_cursor.compare_exchange_strong(current, current->m_next, mo::acq_rel, mo::acquire);
        // }}} v3
            if (current->m_state.compare_exchange_strong(state, state::cons_locked, mo::acq_rel, mo::acquire)) {
                return { this, current };
            }
        }

        return {};
    }

    template<std::default_initializable T, signed S, signed L, bool C, unsigned A, queue_growth_policy G>
    requires (S > 1) && (L > 0) && (A > 0)
    bool dynamic_fast_mpmc_queue<T, S, L, C, A, G>::grow() noexcept {
        std::scoped_lock lock { m_spinlock };

        if (m_free.load(mo::acquire)) {
            return true;
        }

        auto last_slot = m_last_block->last_slot();
        try {
            m_last_block->m_next = new block {};
            last_slot->m_next = m_last_block->m_next->assemble(last_slot->m_next);
        } catch (...) {
            last_slot->m_next = m_first_block->first_slot();
            m_last_block->m_next = nullptr;
            return false;
        }

        m_last_block = m_last_block->m_next;
        m_capacity.fetch_add(S, mo::release);
        m_free.fetch_add(S, mo::acq_rel);
        m_producer.m_cursor.store(last_slot->m_next, mo::release);
        return true;
    }

    template<class T>
    concept any_dynamic_fast_mpmc_queue = requires(T t) {
        [] <std::default_initializable U, int32_t S, int32_t L, bool C, unsigned A, queue_growth_policy G>
        (dynamic_fast_mpmc_queue<U, S, L, C, A, G> &) {} (t);
    };
}
