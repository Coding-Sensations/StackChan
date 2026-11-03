#include "companion/companion_adapter_contract.hpp"
#include <cassert>
#include <cstring>
int main(){using namespace companion::adapter;using P=companion_modes::Provider;
 auto mira=Describe(P::Mira);assert(mira.kind==Kind::RememberedMira&&!mira.a2a_available);
 auto muse=Describe(P::Muse);assert(muse.kind==Kind::DirectMuseText&&muse.private_setup&&muse.text_route_present&&!muse.voice_route_present&&!muse.a2a_available);
 assert(std::strstr(muse.setup,"Private Muse setup")&&std::strstr(muse.setup,"Voice"));
 for(auto p:{P::Hermes,P::Claude,P::Codex}){auto d=Describe(p);assert(d.kind==Kind::UnavailableA2a&&!d.private_setup&&!d.text_route_present&&!d.voice_route_present&&!d.a2a_available);UnavailableConversationPort port(p);assert(port.Provider()==p);assert(port.Connect()==Result::Unavailable);assert(port.SendText("synthetic")==Result::Unavailable);assert(port.StartVoice()==Result::Unavailable);auto s=port.Read();assert(!s.verified&&!s.local_communication_current&&s.communication==companion::room::Communication::Idle);port.Close();assert(!port.Read().verified);}
 assert(Describe(P::StackChan).kind==Kind::ExistingStock);
 assert(Describe(static_cast<P>(99)).kind==Kind::NotBuilt);
}
