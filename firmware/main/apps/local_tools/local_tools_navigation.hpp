#pragma once
#include "local_tools_model.hpp"
#include <array>
namespace local_tools_ui {
enum class Launcher { Pet, Audio };
constexpr Screen EntryScreen(Launcher kind){return kind==Launcher::Pet?Screen::Pet:Screen::Audio;}
constexpr const char* LauncherName(Launcher kind){return kind==Launcher::Pet?"PET":"AUDIO TOOLS";}
constexpr bool ExitLauncher(Launcher,Screen){return true;}
enum class GameRoute { Collection, Game2048, Dice };
struct GameEntry { const char* label; GameRoute route; int action; };
inline constexpr std::array<GameEntry,2> GameCollection{{{"2048",GameRoute::Game2048,1010},{"DnD dice",GameRoute::Dice,1011}}};
constexpr GameRoute SelectGame(int action){for(auto game:GameCollection)if(game.action==action)return game.route;return GameRoute::Collection;}
}
