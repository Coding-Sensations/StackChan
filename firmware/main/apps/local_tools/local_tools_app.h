#pragma once
#include <mooncake.h>
#include <memory>
namespace local_tools_ui {
std::unique_ptr<mooncake::AppAbility> MakeApp();
std::unique_ptr<mooncake::AppAbility> MakePetApp();
std::unique_ptr<mooncake::AppAbility> MakeAudioToolsApp();
}
