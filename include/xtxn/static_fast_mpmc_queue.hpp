// Copyright (c) 2025-2026 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <cassert>
#include <concepts>
#include <limits>
#include "types.hpp"
#include "algo.hpp"
#include "fast_mpmc_queue_commons.hpp"

namespace xtxn {
    template<
        std::default_initializable T,
        signed S,
        bool C = queue_default_auto_completion,
        unsigned A = queue_default_attempts
    >
    requires (S > 1) && (A > 0)
    class alignas(true_sharing_align) static_fast_mpmc_queue {
        using slot_completion = queue_slot_completion<C>;
        class producer_accessor;
        class consumer_accessor;
        using mo = std::memory_order;
        using state = queue_slot_state;

        static constexpr bool c_ntdct = std::is_nothrow_default_constructible_v<T>;

        struct alignas(false_sharing_align) {
            std::atomic_uint_fast64_t m_index { 0 };
            std::atomic_flag m_enable {};
        } m_producer;
        struct alignas(false_sharing_align) {
            std::atomic_uint_fast64_t m_index { 0 };
            std::atomic_flag m_enable {};
        } m_consumer;
        alignas(false_sharing_align) std::atomic_int_fast32_t m_free { S };
        alignas(false_sharing_align) std::atomic<state> m_state[static_cast<size_t>(S)] {};
        alignas(false_sharing_align) T m_payload[static_cast<size_t>(S)] {};

    public:
        using payload_type [[maybe_unused]] = T;
        using size_type = decltype(S);
        using offset_type = decltype(m_producer.m_index)::value_type;

        static constexpr offset_type c_invalid_index { std::numeric_limits<offset_type>::max() };
        static constexpr size_type c_size [[maybe_unused]] { S };
        static constexpr bool c_auto_complete [[maybe_unused]] { C };
        static constexpr unsigned c_default_attempts [[maybe_unused]] { A };

        static_fast_mpmc_queue() noexcept(c_ntdct);
        static_fast_mpmc_queue(const static_fast_mpmc_queue &) = delete;
        static_fast_mpmc_queue(static_fast_mpmc_queue &&) = delete;
        ~static_fast_mpmc_queue() = default;

        static_fast_mpmc_queue & operator=(const static_fast_mpmc_queue &) = delete;
        static_fast_mpmc_queue & operator=(static_fast_mpmc_queue &&) = delete;

        [[nodiscard, maybe_unused]]
        size_type capacity() const noexcept { // NOLINT
            return S;
        }

        [[nodiscard, maybe_unused]]
        size_type free_slots() const noexcept {
            return m_free.load(mo::relaxed);
        }

        [[nodiscard, maybe_unused]]
        bool empty() const noexcept {
            return m_free.load(mo::acquire) == S;
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

    template<std::default_initializable T, signed S, bool C, unsigned A>
    requires (S > 1) && (A > 0)
    class static_fast_mpmc_queue<T, S, C, A>::producer_accessor : public slot_completion {
    protected:
        static_fast_mpmc_queue * const m_queue { nullptr };
        offset_type const m_index { c_invalid_index };

    public:
        producer_accessor() noexcept = default;
        producer_accessor(const producer_accessor &) = delete;
        producer_accessor(producer_accessor &&) = delete;

        producer_accessor(static_fast_mpmc_queue * queue, offset_type index) noexcept
        : slot_completion {}, m_queue { queue }, m_index { index } {
            assert(m_queue);
            assert(m_index < S);
            assert(m_queue->m_state[m_index].load(mo::acquire) == state::prod_locked);
            m_queue->m_free.fetch_sub(1, mo::acq_rel);
        }

        ~producer_accessor() override;

        producer_accessor & operator=(const producer_accessor &) = delete;
        producer_accessor & operator=(producer_accessor &&) = delete;

        [[nodiscard, maybe_unused]]
        T * operator->() noexcept {
            assert(m_queue);
            assert(m_index < S);
            assert(m_queue->m_state[m_index].load(mo::acquire) == state::prod_locked);
            return &m_queue->m_payload[m_index];
        }

        [[nodiscard, maybe_unused]]
        T & operator*() noexcept {
            assert(m_queue);
            assert(m_index < S);
            assert(m_queue->m_state[m_index].load(mo::acquire) == state::prod_locked);
            return m_queue->m_payload[m_index];
        }

        [[nodiscard, maybe_unused]]
        explicit operator bool() noexcept {
            assert(
                (!m_queue && m_index == c_invalid_index)
                || (m_queue && m_index < S && m_queue->m_state[m_index].load(mo::acquire) == state::prod_locked)
            );
            return m_queue;
        }
    };

    template<std::default_initializable T, signed S, bool C, unsigned A>
    requires (S > 1) && (A > 0)
    static_fast_mpmc_queue<T, S, C, A>::producer_accessor::~producer_accessor() {
        if (m_queue) {
            if constexpr (slot_completion::c_auto_complete) {
                m_queue->m_state[m_index].store(state::ready, mo::release);
            } else {
                if (slot_completion::m_complete) {
                    m_queue->m_state[m_index].store(state::ready, mo::release);
                } else {
                    m_queue->m_state[m_index].store(state::free, mo::release);
                    m_queue->m_free.fetch_add(1, mo::acq_rel);
                }
            }
        }
    }

    template<std::default_initializable T, signed S, bool C, unsigned A>
    requires (S > 1) && (A > 0)
    class static_fast_mpmc_queue<T, S, C, A>::consumer_accessor : public slot_completion {
    protected:
        static_fast_mpmc_queue * const m_queue { nullptr };
        offset_type const m_index { c_invalid_index };

    public:
        consumer_accessor() noexcept = default;
        consumer_accessor(const consumer_accessor &) = delete;
        consumer_accessor(consumer_accessor &&) = delete;

        consumer_accessor(static_fast_mpmc_queue * queue, offset_type index) noexcept
        : slot_completion {}, m_queue { queue }, m_index { index } {
            assert(m_queue);
            assert(m_index < S);
            assert(m_queue->m_state[m_index].load(mo::acquire) == state::cons_locked);
        }

        ~consumer_accessor() override;

        consumer_accessor & operator=(const consumer_accessor &) = delete;
        consumer_accessor & operator=(consumer_accessor &&) = delete;

        [[nodiscard, maybe_unused]]
        T * operator->() noexcept {
            assert(m_queue);
            assert(m_index < S);
            assert(m_queue->m_state[m_index].load(mo::acquire) == state::cons_locked);
            return &m_queue->m_payload[m_index];
        }

        [[nodiscard, maybe_unused]]
        T & operator*() noexcept {
            assert(m_queue);
            assert(m_index < S);
            assert(m_queue->m_state[m_index].load(mo::acquire) == state::cons_locked);
            return m_queue->m_payload[m_index];
        }

        [[nodiscard, maybe_unused]]
        explicit operator bool() noexcept {
            assert(
                (!m_queue && m_index == c_invalid_index)
                || (m_queue && m_index < S && m_queue->m_state[m_index].load(mo::acquire) == state::cons_locked)
            );
            return m_queue;
        }
    };

    template<std::default_initializable T, signed S, bool C, unsigned A>
    requires (S > 1) && (A > 0)
    static_fast_mpmc_queue<T, S, C, A>::consumer_accessor::~consumer_accessor() {
        if (m_queue) {
            if constexpr (slot_completion::c_auto_complete) {
                m_queue->m_state[m_index].store(state::free, mo::release);
                m_queue->m_free.fetch_add(1, mo::acq_rel);
            } else {
                if (slot_completion::m_complete) {
                    m_queue->m_state[m_index].store(state::free, mo::release);
                    m_queue->m_free.fetch_add(1, mo::acq_rel);
                } else {
                    m_queue->m_state[m_index].store(state::ready, mo::release);
                }
            }
        }
    }

    template<std::default_initializable T, signed S, bool C, unsigned A>
    requires (S > 1) && (A > 0)
    static_fast_mpmc_queue<T, S, C, A>::static_fast_mpmc_queue() noexcept(c_ntdct) {
        m_producer.m_enable.test_and_set(mo::acquire);
        m_consumer.m_enable.test_and_set(mo::acquire);
    }

    template<std::default_initializable T, signed S, bool C, unsigned A>
    requires (S > 1) && (A > 0)
    auto
    static_fast_mpmc_queue<T, S, C, A>::producer_slot(unsigned slot_acquire_attempts)
    noexcept -> producer_accessor {
        assert(slot_acquire_attempts > 0);

        if (!m_producer.m_enable.test(mo::acquire)) {
            return {};
        }

        do {
            for (
                auto count = S;
                count && m_producer.m_enable.test(mo::acquire) && m_free.load(mo::acquire);
                --count
            ) {
                auto state = state::free;
                auto index = iterate_post_inc<S>(m_producer.m_index);
                if (m_state[index].compare_exchange_strong(state, state::prod_locked, mo::acq_rel, mo::acquire)) {
                    return { this, index };
                }
            }
        } while (--slot_acquire_attempts);

        return {};
    }

    template<std::default_initializable T, signed S, bool C, unsigned A>
    requires (S > 1) && (A > 0)
    auto static_fast_mpmc_queue<T, S, C, A>::consumer_slot() noexcept -> consumer_accessor {
        while (m_consumer.m_enable.test(mo::acquire) && m_free.load(mo::acquire) < S) {
            auto state = state::ready;
            auto index = iterate_post_inc<S>(m_consumer.m_index);
            if (m_state[index].compare_exchange_strong(state, state::cons_locked, mo::acq_rel, mo::acquire)) {
                return { this, index };
            }
        }

        return {};
    }

    template<class T>
    concept any_static_fast_mpmc_queue = requires(T t) {
        [] <std::default_initializable U, int32_t S, bool C, unsigned A> (static_fast_mpmc_queue<U, S, C, A> &) {} (t);
    };
}
