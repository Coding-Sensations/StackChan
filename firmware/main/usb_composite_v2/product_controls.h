#pragma once
#include "startup.h"
#include <mooncake.h>
#include <memory>
namespace usb_composite_v2 {
// Fills callback wiring only. Does not qualify power/identity or start USB.
Integration BindProductCallbacks(Integration qualified);
std::unique_ptr<mooncake::AppAbility> MakeControlsApp();
}
