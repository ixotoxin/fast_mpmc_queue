// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <cassert>
#include <concepts>
#include <atomic>
#include <array>
#include "fast_queue_internal.hpp"
#include "alignment.hpp"

namespace xtxn {
    template<
        std::default_initializable T,
        int32_t S,
        bool C = queue_default_completion,
        unsigned A = queue_default_attempts
    >
    requires ((S >= 4) && (S <= queue_max_capacity_limit) && (A > 0) && (A <= queue_max_attempts))
    class alignas(queue_alignment) fastest_mpmc_queue {
        struct slot;
        using slot_completion = queue_slot_completion<C>;
        // class base_accessor; // CLEANUP
        class producer_accessor;
        class consumer_accessor;
        using mo = std::memory_order;
        using state = queue_slot_state;

        static constexpr bool c_ntdct = std::is_nothrow_default_constructible_v<T>;
        std::array<slot, static_cast<std::size_t>(S)> m_slots {};
        std::atomic_uint_fast32_t m_producer_cursor { 0 };
        std::atomic_uint_fast32_t m_consumer_cursor { 0 };
        std::atomic_int_fast32_t m_free { S };
        std::atomic_bool m_producing { true };
        std::atomic_bool m_consuming { true };

    public:
        using payload_type [[maybe_unused]] = T;
        using size_type = decltype(S);
        using offset_type = decltype(m_producer_cursor)::value_type;

        static constexpr size_type c_size [[maybe_unused]] { S };
        static constexpr bool c_auto_complete [[maybe_unused]] { C };
        static constexpr unsigned c_default_attempts [[maybe_unused]] { A };

        fastest_mpmc_queue() noexcept(c_ntdct) = default;
        fastest_mpmc_queue(const fastest_mpmc_queue &) = delete;
        fastest_mpmc_queue(fastest_mpmc_queue &&) = delete;
        ~fastest_mpmc_queue() noexcept = default;

        fastest_mpmc_queue & operator=(const fastest_mpmc_queue &) = delete;
        fastest_mpmc_queue & operator=(fastest_mpmc_queue &&) = delete;

        [[nodiscard, maybe_unused]]
        size_type capacity() const noexcept {
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
            return m_producing.load(mo::relaxed);
        }

        [[nodiscard, maybe_unused]]
        bool consuming() const noexcept {
            return m_consuming.load(mo::relaxed);
        }

        [[nodiscard]] producer_accessor producer_slot(unsigned = c_default_attempts) noexcept(c_ntdct);
        [[nodiscard]] consumer_accessor consumer_slot(unsigned = c_default_attempts) noexcept;

        [[maybe_unused]]
        void shutdown() noexcept {
            m_producing.store(false, mo::relaxed);
        }

        [[maybe_unused]]
        void stop() noexcept {
            m_producing.store(false, mo::relaxed);
            m_consuming.store(false, mo::relaxed);
        }

    private:
        offset_type iterate_cursor(/*std::atomic_int_fast32_t*/ auto & cursor) {
            /** Pre-increment **/
            // offset_type current { cursor.load(mo::relaxed) };
            // offset_type next;
            // do {
            //     next = (current + 1) % S;
            // } while (!cursor.compare_exchange_weak(current, next, mo::relaxed));
            // return next;

            /** Post-increment **/
            offset_type current { cursor.fetch_add(1, mo::relaxed) };
            offset_type next { current + 1 };
            if (next >= S) {
                cursor.compare_exchange_weak(next, next % S, mo::relaxed);
            }
            if (current >= S) {
                current = current % S;
            }
            return current;
        }
    };

    template<std::default_initializable T, int32_t S, bool C, unsigned A>
    requires ((S >= 4) && (S <= queue_max_capacity_limit) && (A > 0) && (A <= queue_max_attempts))
    struct fastest_mpmc_queue<T, S, C, A>::slot {
        std::atomic<state> m_state { state::free };
        T m_payload {};

        slot() noexcept(c_ntdct) = default;
        slot(const slot &) = delete;
        slot(slot &&) = delete;
        ~slot() noexcept = default;

        slot & operator=(const slot &) = delete;
        slot & operator=(slot &&) = delete;
    };

    // CLEANUP
    // template<std::default_initializable T, int32_t S, bool C, unsigned A>
    // requires ((S >= 4) && (S <= queue_max_capacity_limit) && (A > 0) && (A <= queue_max_attempts))
    // class fastest_mpmc_queue<T, S, C, A>::base_accessor {
    // protected:
    //     fastest_mpmc_queue & m_queue;
    //     slot * const m_slot;
    //
    // public:
    //     base_accessor() = delete;
    //     base_accessor(const base_accessor &) = delete;
    //     base_accessor(base_accessor &&) = delete;
    //
    //     base_accessor(fastest_mpmc_queue & queue, slot * slot) noexcept
    //     : m_queue { queue }, m_slot { slot } {}
    //
    //     ~base_accessor() noexcept = default;
    //
    //     base_accessor & operator=(const base_accessor &) = delete;
    //     base_accessor & operator=(base_accessor &&) = delete;
    //
    //     [[nodiscard, maybe_unused]]
    //     T * operator->() noexcept {
    //         return &(m_slot->m_payload);
    //     }
    //
    //     [[nodiscard, maybe_unused]]
    //     T & operator*() noexcept {
    //         return m_slot->m_payload;
    //     }
    // };

    template<std::default_initializable T, int32_t S, bool C, unsigned A>
    requires ((S >= 4) && (S <= queue_max_capacity_limit) && (A > 0) && (A <= queue_max_attempts))
    class fastest_mpmc_queue<T, S, C, A>::producer_accessor : /*public base_accessor,*/ public slot_completion {
    protected:
        // using base_accessor::m_queue; // CLEANUP
        // using base_accessor::m_slot; // CLEANUP
        fastest_mpmc_queue & m_queue;
        slot * const m_slot;

    public:
        producer_accessor() = delete;
        producer_accessor(const producer_accessor &) = delete;
        producer_accessor(producer_accessor &&) = delete;

        producer_accessor(fastest_mpmc_queue & queue, slot * slot) noexcept
        : /*base_accessor { queue, slot },*/ slot_completion {}, m_queue { queue }, m_slot { slot } {
            if (slot) {
                queue.m_free.fetch_sub(1, mo::acq_rel);
            }
        }

        ~producer_accessor() noexcept;

        producer_accessor & operator=(const producer_accessor &) = delete;
        producer_accessor & operator=(producer_accessor &&) = delete;

        [[nodiscard, maybe_unused]]
        T * operator->() noexcept {
            return &(m_slot->m_payload);
        }

        [[nodiscard, maybe_unused]]
        T & operator*() noexcept {
            return m_slot->m_payload;
        }

        [[nodiscard, maybe_unused]]
        explicit operator bool() noexcept {
            return m_slot && m_slot->m_state.load(mo::acquire) == state::prod_locked;
        }
    };

    template<std::default_initializable T, int32_t S, bool C, unsigned A>
    requires ((S >= 4) && (S <= queue_max_capacity_limit) && (A > 0) && (A <= queue_max_attempts))
    fastest_mpmc_queue<T, S, C, A>::producer_accessor::~producer_accessor() noexcept {
        if (m_slot) {
            if constexpr (slot_completion::c_auto_complete) {
                m_slot->m_state.store(state::ready, mo::release);
            } else {
                if (slot_completion::m_complete) {
                    m_slot->m_state.store(state::ready, mo::release);
                } else {
                    m_queue.m_free.fetch_add(1, mo::acq_rel);
                    m_slot->m_state.store(state::free, mo::release);
                }
            }
        }
    }

    template<std::default_initializable T, int32_t S, bool C, unsigned A>
    requires ((S >= 4) && (S <= queue_max_capacity_limit) && (A > 0) && (A <= queue_max_attempts))
    class fastest_mpmc_queue<T, S, C, A>::consumer_accessor : /*public base_accessor,*/ public slot_completion {
    protected:
        // using base_accessor::m_queue; // CLEANUP
        // using base_accessor::m_slot; // CLEANUP
        fastest_mpmc_queue & m_queue;
        slot * const m_slot;

    public:
        consumer_accessor() = delete;
        consumer_accessor(const consumer_accessor &) = delete;
        consumer_accessor(consumer_accessor &&) = delete;

        consumer_accessor(fastest_mpmc_queue & queue, slot * slot) noexcept
        : /*base_accessor { queue, slot },*/ slot_completion {}, m_queue { queue }, m_slot { slot } {}

        ~consumer_accessor() noexcept;

        consumer_accessor & operator=(const consumer_accessor &) = delete;
        consumer_accessor & operator=(consumer_accessor &&) = delete;

        [[nodiscard, maybe_unused]]
        const T * operator->() noexcept {
            return &(m_slot->m_payload);
        }

        [[nodiscard, maybe_unused]]
        const T & operator*() noexcept {
            return m_slot->m_payload;
        }

        [[nodiscard, maybe_unused]]
        explicit operator bool() noexcept {
            return m_slot && m_slot->m_state.load(mo::acquire) == state::cons_locked;
        }
    };

    template<std::default_initializable T, int32_t S, bool C, unsigned A>
    requires ((S >= 4) && (S <= queue_max_capacity_limit) && (A > 0) && (A <= queue_max_attempts))
    fastest_mpmc_queue<T, S, C, A>::consumer_accessor::~consumer_accessor() noexcept {
        if (m_slot) {
            if constexpr (slot_completion::c_auto_complete) {
                m_queue.m_free.fetch_add(1, mo::acq_rel);
                m_slot->m_state.store(state::free, mo::release);
            } else {
                if (slot_completion::m_complete) {
                    m_queue.m_free.fetch_add(1, mo::acq_rel);
                    m_slot->m_state.store(state::free, mo::release);
                } else {
                    m_slot->m_state.store(state::ready, mo::release);
                }
            }
        }
    }

    template<std::default_initializable T, int32_t S, bool C, unsigned A>
    requires ((S >= 4) && (S <= queue_max_capacity_limit) && (A > 0) && (A <= queue_max_attempts))
    auto fastest_mpmc_queue<T, S, C, A>::producer_slot(unsigned slot_acquire_attempts)
    noexcept(c_ntdct) -> producer_accessor {
        if (!m_free.load(mo::acquire)) {
            return { *this, nullptr };
        }

        unsigned attempts { slot_acquire_attempts - 1 };
        offset_type current { iterate_cursor(m_producer_cursor) };
        offset_type sentinel { current };

        while (m_producing.load(mo::relaxed)) {
            state slot_state { state::free };
            if (
                m_slots[current].m_state.compare_exchange_strong(
                    slot_state, state::prod_locked, mo::acq_rel, mo::acquire
                )
            ) {
                return { *this, &m_slots[current] };
            }
            current = iterate_cursor(m_producer_cursor);

            if (current == sentinel) {
                if (attempts < 1) {
                    break;
                }
                --attempts;
            }
        }
        return { *this, nullptr };
    }

    template<std::default_initializable T, int32_t S, bool C, unsigned A>
    requires ((S >= 4) && (S <= queue_max_capacity_limit) && (A > 0) && (A <= queue_max_attempts))
    auto fastest_mpmc_queue<T, S, C, A>::consumer_slot(unsigned slot_acquire_attempts) noexcept -> consumer_accessor {
        unsigned attempts { slot_acquire_attempts - 1 };
        offset_type current { iterate_cursor(m_consumer_cursor) };
        offset_type sentinel { current };

        while (m_consuming.load(mo::relaxed) && m_free.load(mo::acquire) != S) {
            state slot_state { state::ready };
            if (
                m_slots[current].m_state.compare_exchange_strong(
                    slot_state, state::cons_locked, mo::acq_rel, mo::acquire
                )
            ) {
                return { *this, &m_slots[current] };
            }
            current = iterate_cursor(m_consumer_cursor);

            if (current == sentinel) {
                if (attempts < 1) {
                    break;
                }
                --attempts;
            }
        }

        return { *this, nullptr };
    }

    template<class T>
    concept fastest_mpmc_queue_tc = requires(T t) {
        [] <std::default_initializable U, int32_t S, bool C, unsigned A> (fastest_mpmc_queue<U, S, C, A> &) {} (t);
    };
}
