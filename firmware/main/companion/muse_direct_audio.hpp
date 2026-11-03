// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "audio_owner.hpp"
#include <functional>
namespace companion::muse {
// Resource custody only. This class never opens a codec, starts a microphone,
// enables power or submits voice. Every initially selected grant must remain true.
struct Directions { bool microphone=false; bool output=false; };
class VoiceLease {
    AudioLease lease_;
    std::function<bool()> current_,microphone_,speaker_;
    Directions selected_;
public:
    bool Reserve(uint64_t epoch,std::function<bool()> current,std::function<bool()> microphone,std::function<bool()> speaker){return Reserve(epoch,std::move(current),std::move(microphone),std::move(speaker),{true,true});}
    bool Reserve(uint64_t epoch,std::function<bool()> current,std::function<bool()> microphone,std::function<bool()> speaker,Directions selected){
        if(lease_||!current||!microphone||!speaker||(!selected.microphone&&!selected.output)||!current()||(selected.microphone&&!microphone())||(selected.output&&!speaker()))return false;
        if(!AcquireAudio(AudioOwner::Muse,AudioPurpose::MusePushToTalk,epoch,lease_))return false;
        selected_=selected;current_=std::move(current);microphone_=std::move(microphone);speaker_=std::move(speaker);return true;
    }
    bool Current(){
        const auto captured=lease_;const auto selected=selected_;
        // Keep callback storage alive if a callout stops/replaces this lease.
        const auto current=current_,microphone=microphone_,speaker=speaker_;
        const bool okay=captured&&CommonAudioAuthority().Current(captured)&&current&&current()&&
            (!selected.microphone||(microphone&&microphone()))&&(!selected.output||(speaker&&speaker()))&&
            lease_.generation()==captured.generation()&&lease_.epoch()==captured.epoch()&&CommonAudioAuthority().Current(captured);
        if(!okay&&captured){RevokeAudio(captured);}
        return okay;
    }
    AudioLease ResourceLease()const{return lease_;} // opaque custody, never native consent
    bool Stop(bool directions_closed_and_worker_joined){if(!lease_)return true;RevokeAudio(lease_);if(!ReleaseAudio(lease_,directions_closed_and_worker_joined))return false;current_={};microphone_={};speaker_={};selected_={};return true;}
};
}
