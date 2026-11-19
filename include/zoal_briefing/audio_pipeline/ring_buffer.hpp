#ifndef ZOAL_BRIEFING_RING_BUFFER_HPP
#define ZOAL_BRIEFING_RING_BUFFER_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace zoal_briefing::audio {

// Single-producer, single-consumer sample ring: the capture callback (an
// audio thread that must never block) pushes, the voice loop's worker pops.
// When full, new samples are dropped and counted.
class SampleRing {
public:
  explicit SampleRing(std::size_t capacity_pow2 = 1u << 19) // ~32 s at 16 kHz
      : buf_(capacity_pow2), mask_(capacity_pow2 - 1) {}

  // Producer only.
  void push(const std::int16_t *data, std::size_t count) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    const std::size_t free_space = buf_.size() - (head - tail);
    if (count > free_space) {
      dropped_.fetch_add(count - free_space, std::memory_order_relaxed);
      count = free_space;
    }
    for (std::size_t i = 0; i < count; ++i) buf_[(head + i) & mask_] = data[i];
    head_.store(head + count, std::memory_order_release);
  }

  // Consumer only.
  std::size_t available() const {
    return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_relaxed);
  }

  // Consumer only. Pops exactly `count` samples if available.
  bool pop(std::int16_t *out, std::size_t count) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (head_.load(std::memory_order_acquire) - tail < count) return false;
    for (std::size_t i = 0; i < count; ++i) out[i] = buf_[(tail + i) & mask_];
    tail_.store(tail + count, std::memory_order_release);
    return true;
  }

  // Consumer only: throw away everything buffered.
  void discard() { tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release); }

  std::size_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
  std::vector<std::int16_t> buf_;
  std::size_t mask_;
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
  std::atomic<std::size_t> dropped_{0};
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_RING_BUFFER_HPP
