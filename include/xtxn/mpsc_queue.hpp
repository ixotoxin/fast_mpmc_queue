// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <atomic>

namespace xtxn {
    template<typename T>
    class alignas(std::hardware_constructive_interference_size) mpsc_queue final {
        struct node;

        std::atomic<node *> m_head;
        std::atomic<node *> m_tail;
        bool m_producing { true };
        bool m_consuming { true };

    public:
        mpsc_queue() : m_head { new node }, m_tail { m_head.load(std::memory_order_relaxed) } {}
        mpsc_queue(const mpsc_queue &) = delete;
        mpsc_queue(mpsc_queue && other) = delete;
        ~mpsc_queue();

        mpsc_queue & operator=(const mpsc_queue &) = delete;
        mpsc_queue & operator=(mpsc_queue&& other) = delete;

        [[nodiscard, maybe_unused]]
        bool empty() const noexcept {
            return m_head.load(std::memory_order_acquire)->m_next.load(std::memory_order_acquire) == nullptr;
            // return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
        }

        [[nodiscard, maybe_unused]] bool producing() const noexcept { return m_producing; }
        [[nodiscard, maybe_unused]] bool consuming() const noexcept { return m_consuming; }

        template <typename U> void enqueue(U &&);
        [[nodiscard]] std::unique_ptr<T> dequeue();

        [[maybe_unused]] void shutdown() noexcept { m_producing = false; }
        [[maybe_unused]] void stop() noexcept { m_producing = false; m_consuming = false; }
    };

    template<typename T>
    struct mpsc_queue<T>::node {
        std::unique_ptr<T> m_data;
        std::atomic<node*> m_next;

        node() : m_data { nullptr }, m_next { nullptr } {}

        template <typename U>
        explicit node(U && value) // NOLINT(*-forwarding-reference-overload)
        : m_data { std::make_unique<T>(std::forward<U>(value)) }, m_next { nullptr } {}
    };

    template<typename T>
    mpsc_queue<T>::~mpsc_queue() {
        node * current { m_head.load(std::memory_order_relaxed) };
        while (current) {
            node * next { current->m_next.load(std::memory_order_relaxed) };
            delete current;
            current = next;
        }
    }

    template<typename T>
    template<typename U>
    void mpsc_queue<T>::enqueue(U && value) {
        node * new_node { new node(std::forward<U>(value)) };
        node * prev_tail { m_tail.exchange(new_node, std::memory_order_acq_rel) };
        prev_tail->m_next.store(new_node, std::memory_order_release);
    }

    template<typename T>
    std::unique_ptr<T> mpsc_queue<T>::dequeue() {
        node * next { m_head.load(std::memory_order_acquire)->m_next.load(std::memory_order_acquire) };
        if (!next) {
            return { nullptr };
        }
        node * prev_head { m_head.exchange(next, std::memory_order_acq_rel) };
        std::unique_ptr<T> result { std::move(next->m_data) };
        delete prev_head;
        return result;
    }
}
