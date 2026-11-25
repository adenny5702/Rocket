#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

template <typename T, size_t Capacity> class SpscQueue {
    static_assert(std::has_single_bit(Capacity) && Capacity > 1,
                  "SpscQueue Capacity must be a power of two and be greater than 1");
    static_assert(std::is_trivially_copyable_v<T>, "SpscQueue elements must be trivially copyable");

public:
    constexpr SpscQueue() noexcept = default;

    SpscQueue(const SpscQueue &)            = delete;
    SpscQueue &operator=(const SpscQueue &) = delete;

    [[nodiscard]] bool tryPush(const T &item) noexcept {
        const size_t head      = m_head.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & kMask;

        if (next_head == m_tail.load(std::memory_order_acquire)) {
            m_droppedCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        m_buffer[head] = item;
        m_head.store(next_head, std::memory_order_release);
        return true;
    };

    [[nodiscard]] bool tryPop(T &out_item) noexcept {
        const size_t tail = m_tail.load(std::memory_order_relaxed);

        if (tail == m_head.load(std::memory_order_acquire)) {
            return false;
        }

        out_item = m_buffer[tail];
        m_tail.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    };

    [[nodiscard]] size_t popBatch(T *out_items, size_t max_count) noexcept {
        size_t count = 0;
        while (count < max_count && tryPop(out_items[count])) {
            count++;
        }
        return count;
    };

    [[nodiscard]] size_t sizeApprox() const noexcept {
        const size_t head = m_head.load(std::memory_order_acquire);
        const size_t tail = m_tail.load(std::memory_order_acquire);
        return (head - tail) & kMask;
    };

    [[nodiscard]] uint32_t droppedCount() const noexcept {
        return m_droppedCount.load(std::memory_order_relaxed);
    };

private:
    static constexpr size_t kMask = Capacity - 1;

    alignas(32) std::atomic<size_t> m_head{0};
    alignas(32) std::atomic<size_t> m_tail{0};
    std::atomic<uint32_t> m_droppedCount{0};

    std::array<T, Capacity> m_buffer{};
};
