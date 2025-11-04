// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

/**
 * Implementation of an MPMC Queue with epoch-based reclamation.
 * LIMITATION: This implementation limits the total number of 'enqueue' and 'dequeue' calls to UINT64_MAX - 1.
 */

#pragma once

#include <cassert>
#include <atomic>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include "spinlock.hpp"

namespace xtxn {
    constexpr int64_t queue_default_purge_counter { 0x80 };
    constexpr bool queue_default_purge_thread { true };

    template<
        typename T,
        int64_t PURGE_COUNTER = queue_default_purge_counter,
        bool PURGE_THREAD = queue_default_purge_thread
    >
    class alignas(std::hardware_constructive_interference_size) mpmc_queue final {
        struct node;
        using mo = std::memory_order;

        std::atomic<node *> m_head;
        std::atomic<node *> m_tail;
        std::atomic<node *> m_deleted { nullptr };
        std::unordered_map<std::thread::id, uint_fast64_t> m_thread_epoch {};
        std::atomic_int_fast64_t m_purge_counter { PURGE_COUNTER };
        std::atomic_uint_fast64_t m_epoch {};
        spinlock<spin::yield_thread> m_purge_sl {};
        spinlock<spin::active> m_epoch_sl {};
        std::atomic_bool m_producing { true };
        std::atomic_bool m_consuming { true };

    public:
        mpmc_queue();
        mpmc_queue(const mpmc_queue &) = delete;
        mpmc_queue(mpmc_queue && other) = delete;
        ~mpmc_queue();

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

        template <typename U> bool enqueue(U &&);
        [[nodiscard]] std::unique_ptr<T> dequeue();
        void purge();

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

    template<typename T, int64_t PURGE_COUNTER, bool PURGE_THREAD>
    struct mpmc_queue<T, PURGE_COUNTER, PURGE_THREAD>::node final {
        static constexpr uint_fast64_t c_active { std::numeric_limits<uint_fast64_t>::max() };
        std::unique_ptr<T> m_data;
        std::atomic<node *> m_next { nullptr };
        std::atomic<node *> m_next_deleted { nullptr };
        std::atomic_uint_fast64_t m_deleted_at { c_active };

        node() : m_data { nullptr } {}
        node(const node &) = delete;
        node(node && other) = delete;

        template <typename U>
        explicit node(U && value) // NOLINT(*-forwarding-reference-overload)
        : m_data { std::make_unique<T>(std::forward<U>(value)) } {}

        ~node() = default;

        node & operator=(const node &) = delete;
        node & operator=(node && other) = delete;
    };

    template<typename T, int64_t PURGE_COUNTER, bool PURGE_THREAD>
    mpmc_queue<T, PURGE_COUNTER, PURGE_THREAD>::mpmc_queue()
    : m_head { new node }, m_tail { m_head.load(mo::relaxed) } {
        if constexpr (PURGE_THREAD) {
            std::thread(
                [queue = this] {
                    while (queue->m_consuming.load(mo::acquire)) {
                        while (queue->m_purge_counter.load(mo::acquire) > 0) {
                            std::this_thread::yield();
                            if (!queue->m_consuming.load(mo::acquire)) {
                                break;
                            }
                        }
                        queue->purge();
                        queue->m_purge_counter.store(PURGE_COUNTER, mo::release);
                    }
                }
            ).detach();
        }
    }

    template<typename T, int64_t PURGE_COUNTER, bool PURGE_THREAD>
    mpmc_queue<T, PURGE_COUNTER, PURGE_THREAD>::~mpmc_queue() {
        stop();

        scoped_lock lock { m_purge_sl };

        node * curr { m_head.load(mo::relaxed)->m_next.load(mo::relaxed) };
        while (curr) {
            node * next { curr->m_next.load(mo::relaxed) };
            if (curr->m_deleted_at.load(mo::acquire) == node::c_active) {
                delete curr;
            }
            curr = next;
        }

        delete m_head.load();

        curr = m_deleted.load(mo::relaxed);
        while (curr) {
            node * next { curr->m_next_deleted.load(mo::relaxed) };
            delete curr;
            curr = next;
        }
    }

    template<typename T, int64_t PURGE_COUNTER, bool PURGE_THREAD>
    template<typename U>
    bool mpmc_queue<T, PURGE_COUNTER, PURGE_THREAD>::enqueue(U && value) {
        if (!m_producing.load(mo::relaxed)) {
            return false;
        }

        uint_fast64_t * epoch { nullptr };
        {
            scoped_lock lock { m_epoch_sl };
            epoch = &m_thread_epoch[std::this_thread::get_id()];
        }
        *epoch = m_epoch.fetch_add(1, mo::relaxed);

        node * new_node { new node(std::forward<U>(value)) };

        while (m_producing.load(mo::relaxed)) {
            node * tail { m_tail.load(mo::acquire) };
            node * next { tail->m_next.load(mo::acquire) };

            if (next) {
                m_tail.store(next, mo::release);
                continue;
            }

            if (tail->m_next.compare_exchange_strong(next, new_node, mo::acq_rel, mo::acquire)) {
                m_tail.store(new_node, mo::release);
                break;
            }
        }

        *epoch = node::c_active;
        return true;
    }

    template<typename T, int64_t PURGE_COUNTER, bool PURGE_THREAD>
    std::unique_ptr<T> mpmc_queue<T, PURGE_COUNTER, PURGE_THREAD>::dequeue() {
        if constexpr (PURGE_THREAD) {
            m_purge_counter.fetch_sub(1, mo::acq_rel);
        } else {
            if (m_purge_counter.fetch_sub(1, mo::acq_rel) == 1) {
                purge();
                m_purge_counter.store(PURGE_COUNTER, mo::release);
            }
        }

        uint_fast64_t * epoch { nullptr };
        {
            scoped_lock lock { m_epoch_sl };
            epoch = &m_thread_epoch[std::this_thread::get_id()];
        }
        *epoch = m_epoch.fetch_add(1, mo::relaxed);

        while (m_consuming.load(mo::relaxed)) {
            node * head { m_head.load(mo::acquire) };
            node * first { head->m_next.load(mo::acquire) };

            if (first == nullptr) {
                *epoch = node::c_active;
                return { nullptr };
            }

            if (m_tail.load(mo::acquire) == head) {
                m_tail.store(first, mo::release);
                continue;
            }

            if (first->m_deleted_at.load(mo::acquire) != node::c_active) {
                continue;
            }

            if (m_head.compare_exchange_strong(head, first, mo::acq_rel, mo::acquire)) {
                auto result = std::move(first->m_data);
                first->m_deleted_at.store(*epoch, mo::release);
                head->m_next_deleted.store(m_deleted.exchange(head, mo::acq_rel), mo::release);
                *epoch = node::c_active;
                return result;
            }
        }

        *epoch = node::c_active;
        return { nullptr };
    }

    template<typename T, int64_t PURGE_COUNTER, bool PURGE_THREAD>
    void mpmc_queue<T, PURGE_COUNTER, PURGE_THREAD>::purge() {
        scoped_lock purge_lock { m_purge_sl };

        constexpr int skip_first { 4 };

        uint_fast64_t min_epoch {};
        {
            scoped_lock epoch_lock { m_epoch_sl };
            auto min_it
                = std::ranges::min_element(
                    m_thread_epoch,
                    {},
                    &std::pair<const std::thread::id, uint_fast64_t>::second
                );
            /*auto min_it
                = std::min_element(
                    m_thread_epoch.begin(),
                    m_thread_epoch.end(),
                    [] (const auto & a, const auto & b) { return a.second < b.second; }
                );*/
            if (min_it == m_thread_epoch.end()) {
                return;
            }
            min_epoch = min_it->second;
        }

        int count { skip_first };
        node * last { nullptr };
        node * curr { m_deleted.load(mo::acquire) };
        while (curr && count--) {
            node * next { curr->m_next_deleted.load(mo::acquire) };
            last = curr;
            curr = next;
        }
        if (!last) {
            return;
        }

        while (curr) {
            node * next { curr->m_next_deleted.load(mo::acquire) };
            if (auto deleted_at = curr->m_deleted_at.load(mo::acquire); deleted_at >= min_epoch) {
                last->m_next_deleted.store(curr, mo::release);
                last = curr;
            } else {
                delete curr;
            }
            curr = next;
        }

        last->m_next_deleted.store(nullptr, mo::release);
    }
}
