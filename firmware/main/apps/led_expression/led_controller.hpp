#pragma once
#include "led_service_api.h"
namespace led_expression {
class Output {
public:
  virtual ~Output() = default;
  virtual bool Available() const = 0;
  // true means checked I2C command acceptance, not optical verification.
  virtual bool Submit(const Frame &frame) = 0;
};
class Controller {
  Model model_;
  Fade fade_;
  bool submitted_ = false, fault_ = false;
  Result result_ = Result::Unchanged;
  OutputSource source_ = OutputSource::Stock, accepted_source_ = OutputSource::Stock;
  Frame accepted_{};
  bool has_accepted_ = false;

public:
  Model &Settings() { return model_; }
  Result LastResult() const { return result_; }
  OutputSource Source() const { return source_; }
  CachedPreview Preview() const {
    return {accepted_, result_, accepted_source_, has_accepted_, model_.Off(), model_.Brightness()};
  }
  void Retry() {
    fault_ = false;
    submitted_ = false;
  }
  void Tick(Output &output, uint64_t now) {
    if (fault_) {
      result_ = Result::IoFault;
      return;
    }
    if (!output.Available()) {
      result_ = Result::Unavailable;
      return;
    }
    auto desired = model_.Resolve(now);
    source_ = desired.source;
    bool changed =
        fade_.Step(desired.frame, now, desired.source == OutputSource::UserOff);
    if (!changed && submitted_) {
      return; // Preserve the last checked output result on an idle tick.
    }
    if (!output.Submit(fade_.Current())) {
      fault_ = true;
      result_ = Result::IoFault;
      return;
    }
    accepted_ = fade_.Current();
    accepted_source_ = source_;
    has_accepted_ = true;
    submitted_ = true;
    result_ = Result::Submitted;
  }
};
} // namespace led_expression
