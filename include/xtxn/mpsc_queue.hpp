// Copyright (c) 2025-2026 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <memory>
#include "types.hpp"

namespace xtxn {
    template<typename T>
    class alignas(hw_cis) mpsc_queue final {
        struct node;
        using mo = std::memory_order;

        std::atomic<node *> m_head;
        std::atomic<node *> m_tail;
        alignas(hw_dis) std::atomic_flag m_producing {};
        alignas(hw_dis) std::atomic_flag m_consuming {};

    public:
        mpsc_queue();
        mpsc_queue(const mpsc_queue &) = delete;
        mpsc_queue(mpsc_queue && other) = delete;
        ~mpsc_queue();

        mpsc_queue & operator=(const mpsc_queue &) = delete;
        mpsc_queue & operator=(mpsc_queue&& other) = delete;

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
    struct mpsc_queue<T>::node final {
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
    mpsc_queue<T>::mpsc_queue() : m_head { new node }, m_tail { m_head.load(mo::relaxed) } {
        m_producing.test_and_set(mo::acquire);
        m_consuming.test_and_set(mo::acquire);
    }

    template<typename T>
    mpsc_queue<T>::~mpsc_queue() {
        stop();

        node * current { m_head.load(mo::relaxed) };
        while (current) {
            node * next { current->m_next.load(mo::relaxed) };
            delete current;
            current = next;
        }
    }

    template<typename T>
    template<typename U>
    bool mpsc_queue<T>::enqueue(U && value) {
        if (!m_producing.test(mo::acquire)) {
            return false;
        }

        node * new_node { new node(std::forward<U>(value)) };
        m_tail.exchange(new_node, mo::acq_rel)->m_next.store(new_node, mo::release);

        return true;
    }

    template<typename T>
    [[nodiscard]]
    std::unique_ptr<T> mpsc_queue<T>::dequeue() {
        if (!m_consuming.test(mo::acquire)) {
            return { nullptr };
        }

        node * next { m_head.load(mo::acquire)->m_next.load(mo::acquire) };
        if (!next) {
            return { nullptr };
        }
        delete m_head.exchange(next, mo::acq_rel);

        return std::move(next->m_data);
    }
}
