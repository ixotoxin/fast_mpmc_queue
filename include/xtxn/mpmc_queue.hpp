// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

// ISSUE:
//   Under high worker load, it causes severe memory fragmentation.
//   The code crashes when the number of workers significantly exceeds the CPU core count.

#pragma once

#include <atomic>
#include <memory>
#include "spinlock.hpp"

namespace xtxn {
    template<typename T, int64_t PURGE_TRIGGER = 0x400, int64_t PURGE_DISTANCE = PURGE_TRIGGER << 8>
    class alignas(std::hardware_constructive_interference_size) mpmc_queue final {
        struct node;
        using mo = std::memory_order;

        std::atomic_int_fast64_t m_epoch { 2 };
        std::atomic_int_fast64_t m_purge_trigger { PURGE_TRIGGER };
        std::atomic<node *> m_buffer { nullptr };
        std::atomic<node *> m_head;
        std::atomic<node *> m_tail;
        spinlock<spin::yield_thread> m_spinlock {};
        std::atomic_bool m_producing { true };
        std::atomic_bool m_consuming { true };

        void inc_epoch();
        void purge();

    public:
        mpmc_queue()
        : m_head { new node(m_buffer) }, m_tail { m_head.load(mo::relaxed) } {}

        mpmc_queue(const mpmc_queue &) = delete;
        mpmc_queue(mpmc_queue && other) = delete;

        ~mpmc_queue() {
            stop();
            scoped_lock lock { m_spinlock };
            node * curr { m_buffer.load(mo::relaxed) };
            while (curr) {
                node * next { curr->m_buffer_next.load(mo::relaxed) };
                delete curr;
                curr = next;
            }
        }

        mpmc_queue & operator=(const mpmc_queue &) = delete;
        mpmc_queue & operator=(mpmc_queue && other) = delete;

        [[nodiscard, maybe_unused]]
        bool empty() const noexcept {
            return m_head.load(mo::acquire)->m_next.load(mo::acquire) == nullptr;
            // return m_head.load(mo::acquire) == m_tail.load(mo::acquire);
        }

        [[nodiscard, maybe_unused]]
        bool producing() const noexcept {
            return m_producing.load(mo::relaxed);
        }

        [[nodiscard, maybe_unused]]
        bool consuming() const noexcept {
            return m_consuming.load(mo::relaxed);
        }

        template <typename U> void enqueue(U &&);
        [[nodiscard]] std::unique_ptr<T> dequeue();

        [[maybe_unused]]
        void shutdown() noexcept {
            m_producing.store(false, mo::relaxed);
        }

        [[maybe_unused]]
        void stop() noexcept {
            m_producing.store(false, mo::relaxed);
            m_consuming.store(false, mo::relaxed);
        }

    };

    template<typename T, int64_t PURGE_TRIGGER, int64_t PURGE_DISTANCE>
    struct mpmc_queue<T, PURGE_TRIGGER, PURGE_DISTANCE>::node final {
        std::unique_ptr<T> m_data;
        std::atomic<node *> m_next { nullptr };
        std::atomic<node *> m_buffer_next { nullptr };
        std::atomic_int_fast64_t m_deletion_epoch { 0 };

        node() = delete;
        node(const node &) = delete;
        node(node && other) = delete;
        ~node() = default;

        explicit node(std::atomic<node *> & buffer)
        : m_data { nullptr } {
            m_buffer_next.store(buffer.exchange(this));
        }

        template <typename U>
        explicit node(U && value, std::atomic<node *> & buffer) // NOLINT(*-forwarding-reference-overload)
        : m_data { std::make_unique<T>(std::forward<U>(value)) } {
            m_buffer_next.store(buffer.exchange(this));
        }

        node & operator=(const node &) = delete;
        node & operator=(node && other) = delete;

        void mark_for_delete(int_fast64_t epoch) {
            m_deletion_epoch = epoch;
        }
    };

    template<typename T, int64_t PURGE_TRIGGER, int64_t PURGE_DISTANCE>
    template<typename U>
    void mpmc_queue<T, PURGE_TRIGGER, PURGE_DISTANCE>::enqueue(U && value) {
        if (!m_producing.load(mo::relaxed)) {
            return;
        }

        node * new_node { new node(std::forward<U>(value), m_buffer) };

        while (m_producing.load(mo::relaxed)) {
            node * curr_tail { m_tail.load(mo::acquire) };
            node * next { curr_tail->m_next.load(mo::acquire) };

            if (next != nullptr) {
                m_tail.compare_exchange_strong(curr_tail, next);
                continue;
            }

            node * expected { nullptr };
            if (curr_tail->m_next.compare_exchange_strong(expected, new_node)) {
                m_tail.compare_exchange_strong(curr_tail, new_node);
                return;
            }
        }
    }

    template<typename T, int64_t PURGE_TRIGGER, int64_t PURGE_DISTANCE>
    std::unique_ptr<T> mpmc_queue<T, PURGE_TRIGGER, PURGE_DISTANCE>::dequeue() {
        if (m_purge_trigger.fetch_sub(1, mo::acq_rel) == 1) {
            purge();
            m_purge_trigger.store(PURGE_TRIGGER, mo::release);
        }

        inc_epoch();

        while (m_consuming.load(mo::relaxed)) {
            node * curr_head { m_head.load(mo::acquire) };
            node * curr_tail { m_tail.load(mo::acquire) };
            node * first { curr_head->m_next.load(mo::acquire) };

            if (first == nullptr) {
                return { nullptr };
            }

            if (curr_head == curr_tail) {
                m_tail.compare_exchange_strong(curr_tail, first);
                continue;
            }

            if (m_head.compare_exchange_strong(curr_head, first)) {
                auto result = std::move(first->m_data);
                curr_head->mark_for_delete(m_epoch.load(mo::acquire));
                return result;
            }
        }

        return { nullptr };
    }

    template<typename T, int64_t PURGE_TRIGGER, int64_t PURGE_DISTANCE>
    void mpmc_queue<T, PURGE_TRIGGER, PURGE_DISTANCE>::inc_epoch() {
        if (m_epoch.fetch_add(1, mo::acq_rel) < 0) {
            m_epoch.store(1, mo::release);
        }
    }

    template<typename T, int64_t PURGE_TRIGGER, int64_t PURGE_DISTANCE>
    void mpmc_queue<T, PURGE_TRIGGER, PURGE_DISTANCE>::purge() {
        scoped_lock lock { m_spinlock };
        auto count { PURGE_DISTANCE };
        node * prev { nullptr };
        node * curr { m_buffer.load(mo::acquire) };
        while (curr && count--) {
            prev = curr;
            curr = curr->m_buffer_next.load(mo::acquire);
        }
        if (!curr) {
            return;
        }
        auto epoch0 { m_epoch.load(mo::acquire) };
        while (curr) {
            node * next { curr->m_buffer_next.load(mo::acquire) };
            auto epoch { curr->m_deletion_epoch.load(mo::acquire) };
            auto distance { epoch - epoch0/*m_epoch.load(mo::acquire)*/ };
            if (epoch && (distance < -PURGE_DISTANCE || distance > PURGE_DISTANCE)) {
                prev->m_buffer_next.store(next, mo::release);
                delete curr;
            } else {
                prev = curr;
            }
            curr = next;
        }
    }
}
