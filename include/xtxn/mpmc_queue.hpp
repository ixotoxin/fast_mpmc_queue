// Copyright (c) 2025 Vitaly Anasenko
// Distributed under the MIT License, see accompanying file LICENSE.txt

#pragma once

#include <atomic>
#include <memory>

namespace xtxn {
    template<typename T>
    class alignas(std::hardware_constructive_interference_size) mpmc_queue final {
        struct node;

        std::atomic<std::shared_ptr<node>> m_head;
        std::atomic<std::shared_ptr<node>> m_tail;
        bool m_producing { true };
        bool m_consuming { true };

    public:
        mpmc_queue() : m_head { std::make_shared<node>() }, m_tail { m_head.load(std::memory_order_relaxed) } {}
        mpmc_queue(const mpmc_queue &) = delete;
        mpmc_queue(mpmc_queue && other) = delete;
        ~mpmc_queue() = default;

        mpmc_queue & operator=(const mpmc_queue &) = delete;
        mpmc_queue & operator=(mpmc_queue && other) = delete;

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
    struct alignas(std::hardware_constructive_interference_size) mpmc_queue<T>::node final {
        std::unique_ptr<T> m_data;
        std::atomic<std::shared_ptr<node>> m_next;

        node() : m_data { nullptr }, m_next { nullptr } {}

        template <typename U>
        explicit node(U && value) // NOLINT(*-forwarding-reference-overload)
        : m_data { std::make_unique<T>(std::forward<U>(value)) }, m_next { nullptr } {}
    };

    template<typename T>
    template<typename U>
    void mpmc_queue<T>::enqueue(U && value) {
        auto new_node = std::make_shared<node>(std::forward<U>(value));

        while (true) {
            auto current_tail = m_tail.load(std::memory_order_acquire);
            auto next = current_tail->m_next.load(std::memory_order_acquire);

            if (current_tail == m_tail.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (
                        current_tail->m_next.compare_exchange_weak(
                            next, new_node,
                            std::memory_order_release, std::memory_order_relaxed
                        )
                    ) {
                        m_tail.compare_exchange_strong(
                            current_tail, new_node,
                            std::memory_order_release, std::memory_order_relaxed
                        );
                        return;
                    }
                } else {
                    m_tail.compare_exchange_strong(
                        current_tail, next,
                        std::memory_order_release, std::memory_order_relaxed
                    );
                }
            }
        }
    }

    template<typename T>
    std::unique_ptr<T> mpmc_queue<T>::dequeue() {
        while (true) {
            auto current_head = m_head.load(std::memory_order_acquire);
            auto current_tail = m_tail.load(std::memory_order_acquire);
            auto next = current_head->m_next.load(std::memory_order_acquire);

            if (current_head == m_head.load(std::memory_order_acquire)) {
                if (current_head == current_tail) {
                    if (next == nullptr) {
                        return nullptr;
                    }
                    m_tail.compare_exchange_strong(
                        current_tail, next,
                        std::memory_order_release, std::memory_order_relaxed
                    );
                } else {
                    if (
                        m_head.compare_exchange_strong(
                            current_head, next,
                            std::memory_order_release, std::memory_order_relaxed
                        )
                    ) {
                        std::unique_ptr<T> result = std::move(next->m_data);
                        return result;
                    }
                }
            }
        }
    }
}
