#pragma once
#include <array>
#include <build_profiles/features.hpp>
#include <string_view>

namespace companion_modes {
enum class Provider { StackChan, Mira, Hermes, Muse, Claude, Codex };
enum class Route { Stock, RememberedMira, AdapterRequired };
struct Descriptor { Provider provider; const char* label; Route route; const char* world_key; const char* setup; };
inline constexpr std::array<Descriptor,2+STACKCHAN_BUILD_HERMES+STACKCHAN_BUILD_MUSE+STACKCHAN_BUILD_CLAUDE+STACKCHAN_BUILD_CODEX> Catalog{{
    {Provider::StackChan,"StackChan",Route::Stock,"stock.default","Existing StackChan agent"},
    {Provider::Mira,"Mira",Route::RememberedMira,"mira.room","Your remembered Mac companion"},
#if STACKCHAN_BUILD_HERMES
    {Provider::Hermes,"Hermes",Route::AdapterRequired,"hermes.room","Hermes chat setup is not available here yet."},
#endif
#if STACKCHAN_BUILD_MUSE
    {Provider::Muse,"Muse",Route::AdapterRequired,"muse.room","Private Muse setup: text only. Voice unavailable."},
#endif
#if STACKCHAN_BUILD_CLAUDE
    {Provider::Claude,"Claude",Route::AdapterRequired,"claude.room","Claude chat setup is not available here yet."},
#endif
#if STACKCHAN_BUILD_CODEX
    {Provider::Codex,"Codex",Route::AdapterRequired,"codex.room","Codex chat setup is not available here yet."},
#endif
}};
constexpr const Descriptor* Find(Provider provider) {
    for(const auto& item:Catalog)if(item.provider==provider)return &item;
    return nullptr;
}
constexpr bool IsCompanion(std::string_view name) {
    if(name=="Stackchan")return true; // Preserve existing saved/installed label.
    for(const auto& item:Catalog)if(name==item.label)return true;
    return false;
}
}
