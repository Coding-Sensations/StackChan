#pragma once
#include <memory>
#include <mooncake.h>
#include "persistence_gate.h"
namespace setup_workers { class WorkerBase; }
namespace network_setup {
// Active native Onboarding may open its five-minute phone persistence scope.
// The standalone Wi-Fi controller opens its own scope only on explicit Start.
bool BeginOriginalOnboardingSession();
void EndOriginalOnboardingSession();
// Both entry points open a stopped preview; radios start only on a Start button.
std::unique_ptr<mooncake::AppAbility> MakeApp();
std::unique_ptr<setup_workers::WorkerBase> MakeWorker();
// Open the stopped preview after an intentional shared-driver reset, never start it.
bool ConsumePreviewRequest();
}
