#pragma once
#include <mooncake.h>
#include <memory>
namespace stock_agent {
std::unique_ptr<mooncake::AppAbility> MakeApp();
// Run only after this consumes an explicit physical Start and Mooncake is destroyed.
bool ConsumeStartRequest();
// One-shot stopped preview, never an active session, on intentional software reset only.
bool ConsumePreviewRequest();
// A legacy boot_ai flag alone never authorizes boot activation. Software resets,
// including Stop/Home, always return to the stopped launcher.
bool AutoStartRequested();
// One-shot authorization for the root boot dispatcher after validating opt-in.
void AuthorizeAutoStart();
void RunStockAgent();
}
