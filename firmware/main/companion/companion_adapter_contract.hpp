// SPDX-License-Identifier: MIT
#pragma once
#include <apps/stock_shell/provider_catalog.hpp>
#include "provider_room_state.hpp"
#include <string_view>
namespace companion::adapter {
enum class Kind {NotBuilt,ExistingStock,RememberedMira,DirectMuseText,UnavailableA2a};
// Describes compiled source capabilities, never runtime connection/authority.
struct Readiness {
    Kind kind=Kind::NotBuilt;
    bool private_setup=false,text_route_present=false,voice_route_present=false,a2a_available=false;
    const char* setup="This companion is not included in this build.";
};
inline constexpr Readiness Describe(companion_modes::Provider provider){
    using P=companion_modes::Provider;
    const auto* item=companion_modes::Find(provider);
    if(!item)return {};
    switch(provider){
    case P::StackChan:return {Kind::ExistingStock,false,false,true,false,item->setup};
    case P::Mira:return {Kind::RememberedMira,false,false,true,false,item->setup};
    case P::Muse:return {Kind::DirectMuseText,true,true,false,false,item->setup};
    case P::Hermes:case P::Claude:case P::Codex:return {Kind::UnavailableA2a,false,false,false,false,item->setup};
    }
    return {};
}
enum class Result {Unavailable};
// Explicit inert seam until a separately reviewed real adapter is installed.
// No transport, credentials, callbacks, signer or caller authority flags.
class UnavailableConversationPort {
    companion_modes::Provider provider_;
public:
    explicit constexpr UnavailableConversationPort(companion_modes::Provider provider):provider_(provider){}
    constexpr companion_modes::Provider Provider()const{return provider_;}
    constexpr Result Connect()const{return Result::Unavailable;}
    constexpr Result SendText(std::string_view)const{return Result::Unavailable;}
    constexpr Result StartVoice()const{return Result::Unavailable;}
    room::Snapshot Read()const{return {};}
    constexpr void Close()const{}
};
}
