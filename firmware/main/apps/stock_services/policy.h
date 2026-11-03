#pragma once
#include <optional>

namespace stock_services {
enum class Feature {
    Avatar, EspNow, EzData, Dance, Servo, Microphone, Rgb, Account, FirmwareCheck, AppCenter, Onboarding
};
// Policy is inactive in previews and becomes active only after an explicit Start
// request is consumed and the isolated native runtime acquires its ownership.
std::optional<Feature> CurrentFeature();
bool IsActive();
bool AllowsCamera();
}
