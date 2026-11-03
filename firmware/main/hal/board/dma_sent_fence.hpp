#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#ifndef CODEC_FENCE_IRAM
#define CODEC_FENCE_IRAM
#endif
namespace codec_drain {
// Pure bounded tracker; caller serializes Begin/OnSent/Fail/Cancel under the
// codec-owned ISR/task critical section. No IO, wait, callback registration,
// channel custody or acoustic-completion claim is hidden here.
class DmaSentFence {
 public:
  static constexpr std::size_t kDescriptors = 6; // pinned AudioCodec config
  enum class State { Idle, Pending, Sent, Failed, Cancelled };
  bool Begin(std::uint64_t generation) {
    if (!generation || generation <= generation_ || state_ == State::Pending) return false;
    generation_ = generation; buffers_.fill(0); counts_.fill(0); state_ = State::Pending;
    return true;
  }
  bool CODEC_FENCE_IRAM OnSent(std::uint64_t generation, std::uintptr_t buffer, std::size_t size) {
    if (generation != generation_ || state_ != State::Pending) return false;
    if (!buffer || !size) { state_ = State::Failed; return false; }
    std::size_t i = 0;
    while (i < kDescriptors && buffers_[i] && buffers_[i] != buffer) ++i;
    if (i == kDescriptors) { state_ = State::Failed; return false; }
    if (!buffers_[i]) buffers_[i] = buffer;
    if (counts_[i] < 2) ++counts_[i];
    // Two sent observations of EACH descriptor conservatively exclude one
    // callback from a descriptor completed just before arm. CoreS3 auto-clear
    // remains enabled and no additional writer may run while this fence owns TX.
    for (auto count : counts_) if (count < 2) return false;
    state_ = State::Sent; return true;
  }
  void Fail(std::uint64_t generation) { if (generation == generation_) state_ = State::Failed; }
  void Cancel(std::uint64_t generation) { if (generation == generation_) state_ = State::Cancelled; }
  bool Sent(std::uint64_t generation) const { return generation == generation_ && state_ == State::Sent; }
  State state() const { return state_; }
 private:
  std::uint64_t generation_ = 0;
  State state_ = State::Idle;
  std::array<std::uintptr_t, kDescriptors> buffers_{};
  std::array<unsigned char, kDescriptors> counts_{};
};
} // namespace codec_drain
