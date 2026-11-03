#pragma once
#include <array>
#include "../stock_shell/provider_catalog.hpp"
#include <string_view>
namespace launcher_categories {
enum class Group { Companions, Pet, Games, Music, RemotesHome, Network, Settings };
inline constexpr std::array<std::string_view,7> Titles{{"Companions","Avatar & Motion","Games","Music & Audio","Remotes & Home","Network Tools","Settings"}};
constexpr Group Assign(std::string_view name){
 if(name=="AVATAR"||name=="DANCE")return Group::Pet;
 if(name=="PET"||name=="GAMES")return Group::Games;
 if(name=="MUSIC & AUDIO"||name=="AUDIO TOOLS"||name=="SYNTH"||name=="GROOVEBOX"||name=="PAD SAMPLER")return Group::Music;
 if(name=="LED EXPRESSIONS"||name=="TOOLS"||name=="PROXIMITY")return Group::RemotesHome;
 if(name=="NETWORK TOOLS"||name=="ESPNOW.REMOTE"||name=="EZDATA")return Group::Network;
 if(companion_modes::IsCompanion(name))return Group::Companions;
 return Group::Settings; // Keep future/unrecognized installed entries reachable.
}
constexpr int Token(Group group){return -1000-int(group);}
constexpr int Back=-2000;
constexpr bool IsGroup(int id){return id<=-1000&&id>=-1006;}
constexpr Group Decode(int id){return Group(-1000-id);}
}
