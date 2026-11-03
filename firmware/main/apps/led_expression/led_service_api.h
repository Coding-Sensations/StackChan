#pragma once
#include "led_model.hpp"
namespace led_expression {
enum class Result { Unchanged, Submitted, Unavailable, IoFault };
// These are local device operations, not a network command/authorization API.
bool SetBrightness(unsigned percent);
void SetOff(bool off);
void SetManualColor(Color color);
void ReleaseManualColor();
void PublishStatus(Status status);
void StageStockPixel(unsigned index, Color color);
void Tick(uint64_t now_ms);
Result LastResult();
struct CachedPreview {
  Frame accepted{};
  Result result = Result::Unchanged;
  OutputSource source = OutputSource::Stock;
  bool has_accepted = false, off = false;
  unsigned brightness = 25;
};
// Pure cached snapshot; no hardware query and no optical confirmation.
CachedPreview ReadCachedPreview();
// Explicit local recovery after checked I2C failure; never automatically
// retried.
void RetryOutput();
} // namespace led_expression
