// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

/**
 * Implementation of an MPMC Queue with epoch-based reclamation.
 * LIMITATION: This implementation limits the total number of 'enqueue' and 'dequeue' calls to UINT64_MAX - 1.
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <new>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include "spinlock.hpp"

namespace xtxn {
    constexpr int64_t queue_default_purge_counter { 0x80 };
    constexpr bool queue_default_purge_thread { true };
    constexpr int queue_default_purge_skip_first { 0x80 };

    template<
        typename T,
        int64_t C = queue_default_purge_counter,
        bool H = queue_default_purge_thread,
        int S = queue_default_purge_skip_first
    >
    requires (C >= 4) && (S >= 4)
    class alignas(std::hardware_constructive_interference_size) mpmc_queue final {
        struct node;
        using mo = std::memory_order;
        using epoch_type = uint_fast64_t;

        static constexpr epoch_type c_before_epoch { std::numeric_limits<epoch_type>::min() };
        static constexpr epoch_type c_beyond_epoch { std::numeric_limits<epoch_type>::max() };

        std::unordered_map<std::thread::id, epoch_type> m_thread_epoch {};
        std::atomic<node *> m_head;
        std::atomic<node *> m_tail;
        std::atomic<node *> m_deleted { nullptr };
        std::atomic_int_fast64_t m_purge_counter { C };
        std::atomic<epoch_type> m_epoch { c_before_epoch + 1 };
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
        void touch() {
            scoped_lock epoch_lock { m_epoch_sl };
            m_thread_epoch[std::this_thread::get_id()] = m_epoch.fetch_add(1, mo::relaxed);
        }

        [[maybe_unused]]
        void escape() {
            scoped_lock epoch_lock { m_epoch_sl };
            m_thread_epoch.erase(std::this_thread::get_id());
        }

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

    template<typename T, int64_t C, bool H, int S>
    requires (C >= 4) && (S >= 4)
    struct mpmc_queue<T, C, H, S>::node final {
        std::unique_ptr<T> m_data;
        std::atomic<node *> m_next { nullptr };
        std::atomic<node *> m_next_deleted { nullptr };
        std::atomic<epoch_type> m_deleted_at { c_beyond_epoch };

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

    template<typename T, int64_t C, bool H, int S>
    requires (C >= 4) && (S >= 4)
    mpmc_queue<T, C, H, S>::mpmc_queue()
    : m_head { new node }, m_tail { m_head.load(mo::relaxed) } {
        if constexpr (H) {
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
                        queue->m_purge_counter.store(C, mo::release);
                    }
                }
            ).detach();
        }
    }

    template<typename T, int64_t C, bool H, int S>
    requires (C >= 4) && (S >= 4)
    mpmc_queue<T, C, H, S>::~mpmc_queue() {
        stop();

        scoped_lock lock { m_purge_sl };

        node * current { m_head.load(mo::relaxed)->m_next.load(mo::relaxed) };
        while (current) {
            node * next { current->m_next.load(mo::relaxed) };
            if (current->m_deleted_at.load(mo::acquire) == c_beyond_epoch) {
                delete current;
            }
            current = next;
        }

        delete m_head.load();

        current = m_deleted.load(mo::relaxed);
        while (current) {
            node * next { current->m_next_deleted.load(mo::relaxed) };
            delete current;
            current = next;
        }
    }

    template<typename T, int64_t C, bool H, int S>
    requires (C >= 4) && (S >= 4)
    template<typename U>
    bool mpmc_queue<T, C, H, S>::enqueue(U && value) {
        if (!m_producing.load(mo::relaxed)) {
            return false;
        }

        epoch_type epoch { m_epoch.fetch_add(1, mo::relaxed) };
        assert(epoch != c_beyond_epoch);
        epoch_type * thread_epoch { nullptr };
        {
            scoped_lock lock { m_epoch_sl };
            thread_epoch = &(m_thread_epoch.insert({ std::this_thread::get_id(), c_before_epoch }).first->second);
        }
        /*epoch_type & thread_epoch { std::invoke([queue = this] () -> epoch_type & {
            scoped_lock lock { queue->m_epoch_sl };
            return queue->m_thread_epoch.insert({ std::this_thread::get_id(), c_before_epoch }).first->second;
        }) };*/

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

        *thread_epoch = epoch;
        return true;
    }

    template<typename T, int64_t C, bool H, int S>
    requires (C >= 4) && (S >= 4)
    [[nodiscard]]
    std::unique_ptr<T> mpmc_queue<T, C, H, S>::dequeue() {
        if constexpr (H) {
            m_purge_counter.fetch_sub(1, mo::acq_rel);
        } else {
            if (m_purge_counter.fetch_sub(1, mo::acq_rel) == 1) {
                purge();
                m_purge_counter.store(C, mo::release);
            }
        }

        epoch_type epoch { m_epoch.fetch_add(1, mo::relaxed) };
        assert(epoch != c_beyond_epoch);
        epoch_type * thread_epoch { nullptr };
        {
            scoped_lock lock { m_epoch_sl };
            thread_epoch = &(m_thread_epoch.insert({ std::this_thread::get_id(), c_before_epoch }).first->second);
        }
        /*epoch_type & thread_epoch { std::invoke([queue = this] () -> epoch_type & {
            scoped_lock lock { queue->m_epoch_sl };
            return queue->m_thread_epoch.insert({ std::this_thread::get_id(), c_before_epoch }).first->second;
        }) };*/

        while (m_consuming.load(mo::relaxed)) {
            node * head { m_head.load(mo::acquire) };
            node * first { head->m_next.load(mo::acquire) };

            if (first == nullptr) {
                *thread_epoch = epoch;
                return { nullptr };
            }

            if (m_tail.load(mo::acquire) == head) {
                m_tail.store(first, mo::release);
                continue;
            }

            if (first->m_deleted_at.load(mo::acquire) != c_beyond_epoch) {
                continue;
            }

            if (m_head.compare_exchange_strong(head, first, mo::acq_rel, mo::acquire)) {
                auto result = std::move(first->m_data);
                first->m_deleted_at.store(epoch, mo::release);
                head->m_next_deleted.store(m_deleted.exchange(head, mo::acq_rel), mo::release);
                *thread_epoch = epoch;
                return result;
            }
        }

        *thread_epoch = epoch;
        return { nullptr };
    }

    template<typename T, int64_t C, bool H, int S>
    requires (C >= 4) && (S >= 4)
    void mpmc_queue<T, C, H, S>::purge() {
        scoped_lock purge_lock { m_purge_sl };

        epoch_type min_epoch { m_epoch.load(mo::acquire) };
        {
            scoped_lock epoch_lock { m_epoch_sl };
            auto min_it
                = std::ranges::min_element(
                    m_thread_epoch,
                    {},
                    &std::pair<const std::thread::id, epoch_type>::second
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
            if (min_epoch > min_it->second) {
                min_epoch = min_it->second;
            }
        }

        int count { S };
        node * last { nullptr };
        node * current { m_deleted.load(mo::acquire) };
        while (current && count--) {
            node * next { current->m_next_deleted.load(mo::acquire) };
            last = current;
            current = next;
        }
        if (!last) {
            return;
        }

        while (current) {
            node * next { current->m_next_deleted.load(mo::acquire) };
            if (auto deleted_at = current->m_deleted_at.load(mo::acquire); deleted_at >= min_epoch) {
                last->m_next_deleted.store(current, mo::release);
                last = current;
            } else {
                delete current;
            }
            current = next;
        }

        last->m_next_deleted.store(nullptr, mo::release);
    }
}
