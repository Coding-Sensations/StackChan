#pragma once
#include <algorithm>
#include <apps/network_sensor/expression_inputs.hpp>
#include <array>
#include <cstdint>
namespace led_expression {
constexpr uint8_t
    PixelCount = 12,
    ChannelLimit =
        168; // Existing internal LED tool cap, not an electrical mA guarantee.
struct Color {
  uint8_t red = 0, green = 0, blue = 0;
  bool operator==(const Color &c) const {
    return red == c.red && green == c.green && blue == c.blue;
  }
};
using Frame = std::array<Color, PixelCount>;
enum class Status { None, Pairing, Listening, Speaking, Fault };
enum class OutputSource { Stock, Expression, Manual, Status, UserOff };
struct Desired {
  Frame frame{};
  OutputSource source = OutputSource::Stock;
};
inline Color Safe(Color color) {
  color.red = std::min(color.red, ChannelLimit);
  color.green = std::min(color.green, ChannelLimit);
  color.blue = std::min(color.blue, ChannelLimit);
  return color;
}
inline Color Scale(Color color, unsigned percent) {
  color = Safe(color);
  return {uint8_t(unsigned(color.red) * percent / 100),
          uint8_t(unsigned(color.green) * percent / 100),
          uint8_t(unsigned(color.blue) * percent / 100)};
}
class Model {
  Frame stock_{};
  Color manual_{};
  bool manual_active_ = false, off_ = false;
  unsigned brightness_ = 25;
  Status status_ = Status::None;
  proximity_expression::Expression expression_{};
  uint64_t observed_ = 0;
  static Frame Solid(Color color) {
    Frame f;
    f.fill(color);
    return f;
  }

public:
  bool Brightness(unsigned value) {
    if (value > 100)
      return false;
    brightness_ = value;
    return true;
  }
  unsigned Brightness() const { return brightness_; }
  void Off(bool value) { off_ = value; }
  bool Off() const { return off_; }
  bool StockPixel(unsigned index, Color color) {
    if (index >= PixelCount)
      return false;
    stock_[index] = Safe(color);
    return true;
  }
  void Manual(Color color) {
    manual_ = Safe(color);
    manual_active_ = true;
  }
  void ReleaseManual() { manual_active_ = false; }
  void SetStatus(Status value) { status_ = value; }
  void Expression(proximity_expression::Expression value, uint64_t observed) {
    expression_ = value;
    observed_ = observed;
  }
  Desired Resolve(uint64_t now) const {
    Desired out{stock_, OutputSource::Stock};
    Color color;
    const bool fresh = now >= observed_ && now - observed_ <= 1000;
    if (fresh && expression_.source != proximity_expression::Source::None &&
        expression_.face != proximity_expression::Face::Neutral) {
      out.source = OutputSource::Expression;
      switch (expression_.face) {
      case proximity_expression::Face::Happy:
        color = {40, 120, 60};
        break;
      case proximity_expression::Face::Curious:
        color = {40, 80, 140};
        break;
      case proximity_expression::Face::Sad:
        color = {90, 40, 120};
        break;
      default:
        break;
      }
      // Slow four-second breath, never a blink/strobe. 60..100 percent range.
      auto phase = now % 4000;
      unsigned breath =
          60 + unsigned((phase < 2000 ? phase : 4000 - phase) * 40 / 2000);
      out.frame = Solid(Scale(color, breath));
    }
    if (manual_active_) {
      out = {Solid(manual_), OutputSource::Manual};
    }
    switch (status_) {
    case Status::Pairing:
      color = {100, 80, 0};
      break;
    case Status::Listening:
      color = {0, 90, 140};
      break;
    case Status::Speaking:
      color = {30, 120, 45};
      break;
    case Status::Fault:
      color = {140, 30, 0};
      break;
    default:
      break;
    }
    if (status_ != Status::None)
      out = {Solid(color), OutputSource::Status};
    if (off_)
      out = {Frame{}, OutputSource::UserOff};
    for (auto &pixel : out.frame)
      pixel = Scale(pixel, brightness_);
    return out;
  }
};
// Smooth rate-limited output; no mutation of servo power, LED count or pin
// mode.
class Fade {
  Frame frame_{};
  uint64_t last_ = 0;
  bool started_ = false;

public:
  const Frame &Current() const { return frame_; }
  bool Step(const Frame &target, uint64_t now, bool immediate_off = false) {
    if (immediate_off) {
      Frame zero{};
      if (frame_ == zero)
        return false;
      frame_ = zero;
      last_ = now;
      started_ = true;
      return true;
    }
    if (started_ && (now < last_ || now - last_ < 40))
      return false;
    started_ = true;
    last_ = now;
    bool changed = false;
    auto move = [&](uint8_t &current, uint8_t desired) {
      auto old = current;
      if (current < desired)
        current = uint8_t(std::min(unsigned(desired), unsigned(current) + 8));
      else if (current > desired)
        current = uint8_t(std::max(int(desired), int(current) - 8));
      changed |= current != old;
    };
    for (size_t i = 0; i < PixelCount; ++i) {
      move(frame_[i].red, target[i].red);
      move(frame_[i].green, target[i].green);
      move(frame_[i].blue, target[i].blue);
    }
    return changed;
  }
};
inline std::array<uint8_t, 24> Encode565(const Frame &frame) {
  std::array<uint8_t, 24> bytes{};
  for (size_t i = 0; i < PixelCount; ++i) {
    auto c = Safe(frame[i]);
    uint16_t v =
        ((c.red & 0xf8) << 8) | ((c.green & 0xfc) << 3) | (c.blue >> 3);
    bytes[2 * i] = uint8_t(v);
    bytes[2 * i + 1] = uint8_t(v >> 8);
  }
  return bytes;
}
} // namespace led_expression
