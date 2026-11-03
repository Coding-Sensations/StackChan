#pragma once
#include <mooncake.h>
#include <memory>
namespace instrument_ui {
enum class Kind { Synth, Groovebox, Sampler };
std::unique_ptr<mooncake::AppAbility> MakeApp(Kind);
}
