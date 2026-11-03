#pragma once
#include <apps/stock_services/policy.h>
#include <array>
namespace stock_shell {
using Feature=stock_services::Feature;
inline constexpr std::array<Feature,5> LauncherNativeFeatures{Feature::Avatar,Feature::EspNow,Feature::AppCenter,Feature::EzData,Feature::Dance};
constexpr bool IsSetupFeature(Feature feature) {
 return feature==Feature::Servo||feature==Feature::Microphone||feature==Feature::Rgb||feature==Feature::Account||feature==Feature::FirmwareCheck;
}
}
