// Copyright (c) 2025-2026 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

/**
 * Implementation of an MPMC Queue with deferred deletion.
 */

#pragma once

#include <memory>
#include "types.hpp"
#include "color_barrier.hpp"

namespace xtxn {
    template<typename T>
    class alignas(hw_cis) mpmcdd_queue final {
        struct node;
        using mo = std::memory_order;

        std::atomic<node *> m_head;
        std::atomic<node *> m_tail;
        std::atomic<node *> m_deleted { nullptr };
        color_barrier m_barrier {};
        alignas(hw_dis) std::atomic_flag m_producing {};
        alignas(hw_dis) std::atomic_flag m_consuming {};

    public:
        mpmcdd_queue();
        mpmcdd_queue(const mpmcdd_queue &) = delete;
        mpmcdd_queue(mpmcdd_queue && other) = delete;
        ~mpmcdd_queue();

        mpmcdd_queue & operator=(const mpmcdd_queue &) = delete;
        mpmcdd_queue & operator=(mpmcdd_queue && other) = delete;

        [[nodiscard, maybe_unused]]
        bool empty() const noexcept {
            return m_head.load(mo::acquire)->m_next.load(mo::acquire) == nullptr;
            // return m_head.load(mo::acquire) == m_tail.load(mo::acquire);
        }

        [[nodiscard, maybe_unused]]
        bool producing() const noexcept {
            return m_producing.test(mo::acquire);
        }

        [[nodiscard, maybe_unused]]
        bool consuming() const noexcept {
            return m_consuming.test(mo::acquire);
        }

        template <typename U> bool enqueue(U &&);
        [[nodiscard]] std::unique_ptr<T> dequeue();
        [[maybe_unused]] void purge();

        [[maybe_unused]]
        void shutdown() noexcept {
            m_producing.clear(mo::release);
        }

        [[maybe_unused]]
        void stop() noexcept {
            m_producing.clear(mo::release);
            m_consuming.clear(mo::release);
        }
    };

    template<typename T>
    struct mpmcdd_queue<T>::node final {
        std::unique_ptr<T> m_data;
        std::atomic<node *> m_next { nullptr };
        std::atomic<node *> m_next_deleted { nullptr };
        std::atomic_flag m_deleted {};

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

    template<typename T>
    mpmcdd_queue<T>::mpmcdd_queue() : m_head { new node }, m_tail { m_head.load(mo::relaxed) } {
        m_producing.test_and_set(mo::acquire);
        m_consuming.test_and_set(mo::acquire);
    }

    template<typename T>
    mpmcdd_queue<T>::~mpmcdd_queue() {
        stop();

        red_lock lock { m_barrier };

        node * current { m_head.load(mo::relaxed)->m_next.load(mo::relaxed) };
        while (current) {
            node * next { current->m_next.load(mo::relaxed) };
            if (!current->m_deleted.test()) {
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

    template<typename T>
    template<typename U>
    bool mpmcdd_queue<T>::enqueue(U && value) {
        if (!m_producing.test(mo::acquire)) {
            return false;
        }

        green_lock lock { m_barrier };

        node * new_node { new node(std::forward<U>(value)) };

        while (m_producing.test(mo::acquire)) {
            node * tail { m_tail.load(mo::acquire) };
            node * next { tail->m_next.load(mo::acquire) };

            if (m_tail.load(mo::relaxed) == tail) {
                if (next) {
                    m_tail.store(next, mo::release);
                } else if (tail->m_next.compare_exchange_strong(next, new_node, mo::acq_rel, mo::acquire)) {
                    break;
                }
            }
        }

        return true;
    }

    template<typename T>
    [[nodiscard]]
    std::unique_ptr<T> mpmcdd_queue<T>::dequeue() {
        green_lock lock { m_barrier };

        while (m_consuming.test(mo::acquire)) {
            node * head { m_head.load(mo::acquire) };
            node * first { head->m_next.load(mo::acquire) };

            if (head == m_head.load(mo::relaxed)) {
                if (first == nullptr) {
                    return { nullptr };
                }

                if (m_tail.load(mo::relaxed) == head) {
                    m_tail.compare_exchange_strong(head, first, mo::acq_rel, mo::acquire);
                    continue;
                }

                if (first->m_deleted.test_and_set(mo::acq_rel)) {
                    continue;
                }

                if (m_head.compare_exchange_strong(head, first, mo::acq_rel, mo::acquire)) {
                    auto result = std::move(first->m_data);
                    head->m_next_deleted.store(m_deleted.exchange(head, mo::acq_rel), mo::release);
                    return result;
                }
            }
        }

        return { nullptr };
    }

    template<typename T>
    [[maybe_unused]]
    void mpmcdd_queue<T>::purge() {
        red_lock lock { m_barrier };

        node * current { m_deleted.load(mo::relaxed) };
        while (current && m_consuming.test(mo::acquire)) {
            node * next { current->m_next_deleted.load(mo::relaxed) };
            delete current;
            current = next;
        }
    }
}
