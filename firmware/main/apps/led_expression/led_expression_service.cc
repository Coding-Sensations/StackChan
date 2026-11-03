#include "led_expression_service.h"
#include "led_controller.hpp"
#include <hal/hal.h>
#include <mutex>
namespace led_expression {
namespace {
// One writer for all twelve LEDs. No bus/device creation or power
// configuration.
class HalOutput final : public Output {
  bool Available() const override { return GetHAL().rgbHardwareAvailable(); }
  bool Submit(const Frame &frame) override {
    auto bytes = Encode565(frame);
    return GetHAL().submitRgb565Frame(bytes.data(), bytes.size());
  }
};
struct Runtime {
  std::mutex mutex;
  Controller controller;
  HalOutput output;
};
Runtime &State() {
  static auto *runtime = new Runtime;
  return *runtime;
}
} // namespace
void PublishExpression(proximity_expression::Expression value,
                       uint64_t observed) {
  auto &s = State();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.controller.Settings().Expression(value, observed);
}
bool SetBrightness(unsigned value) {
  auto &s = State();
  std::lock_guard<std::mutex> lock(s.mutex);
  return s.controller.Settings().Brightness(value);
}
void SetOff(bool value) {
  auto &s = State();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.controller.Settings().Off(value);
}
void SetManualColor(Color value) {
  auto &s = State();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.controller.Settings().Manual(value);
}
void ReleaseManualColor() {
  auto &s = State();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.controller.Settings().ReleaseManual();
}
void PublishStatus(Status value) {
  auto &s = State();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.controller.Settings().SetStatus(value);
}
void StageStockPixel(unsigned index, Color value) {
  auto &s = State();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.controller.Settings().StockPixel(index, value);
}
void Tick(uint64_t now) {
  auto &s = State();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.controller.Tick(s.output, now);
}
Result LastResult() {
  auto &s = State();
  std::lock_guard<std::mutex> lock(s.mutex);
  return s.controller.LastResult();
}
CachedPreview ReadCachedPreview() {
  auto &s = State();
  std::lock_guard<std::mutex> lock(s.mutex);
  return s.controller.Preview();
}
void RetryOutput() {
  auto &s = State();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.controller.Retry();
}
} // namespace led_expression
