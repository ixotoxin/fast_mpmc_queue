// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

/**
 * Implementation of an MPMC Queue as an MPSC Queue with a Spinlock.
 */

#pragma once

#include <atomic>
#include <memory>
#include "spinlock.hpp"

namespace xtxn {
    template<typename T>
    class alignas(std::hardware_constructive_interference_size) mpmcsl_queue final {
        struct node;
        using mo = std::memory_order;

        std::atomic<node *> m_head;
        std::atomic<node *> m_tail;
        spinlock<spin::active> m_spinlock {};
        std::atomic_bool m_producing { true };
        std::atomic_bool m_consuming { true };

    public:
        mpmcsl_queue() : m_head { new node }, m_tail { m_head.load(mo::relaxed) } {}
        mpmcsl_queue(const mpmcsl_queue &) = delete;
        mpmcsl_queue(mpmcsl_queue && other) = delete;
        ~mpmcsl_queue();

        mpmcsl_queue & operator=(const mpmcsl_queue &) = delete;
        mpmcsl_queue & operator=(mpmcsl_queue && other) = delete;

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

    template<typename T>
    struct mpmcsl_queue<T>::node final {
        std::unique_ptr<T> m_data;
        std::atomic<node *> m_next { nullptr };

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
    mpmcsl_queue<T>::~mpmcsl_queue() {
        stop();

        scoped_lock lock { m_spinlock };

        node * current { m_head.load(mo::relaxed) };
        while (current) {
            node * next { current->m_next.load(mo::relaxed) };
            delete current;
            current = next;
        }
    }

    template<typename T>
    template<typename U>
    bool mpmcsl_queue<T>::enqueue(U && value) {
        if (!m_producing.load(mo::relaxed)) {
            return false;
        }

        scoped_lock lock { m_spinlock };

        node * new_node { new node(std::forward<U>(value)) };
        m_tail.exchange(new_node, mo::acq_rel)->m_next.store(new_node, mo::release);

        return true;
    }

    template<typename T>
    std::unique_ptr<T> mpmcsl_queue<T>::dequeue() {
        if (!m_consuming.load(mo::relaxed)) {
            return { nullptr };
        }

        scoped_lock lock { m_spinlock };

        node * next { m_head.load(mo::acquire)->m_next.load(mo::acquire) };
        if (!next) {
            return { nullptr };
        }
        delete m_head.exchange(next, mo::acq_rel);

        return std::move(next->m_data);
    }
}
