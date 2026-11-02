// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_codec_owner.hpp"
#include "muse_voice_pcm.hpp"
namespace companion::muse {
// Single-use synchronous codec adapter. No task, native grant or live route.
// Caller serializes operations and retains Core/Clock/Owner until checked release.
template<class Core>
class CoreVoiceCodec final:public voice_codec_prep::Codec {
    Core& core_;
    struct CheckedCore {
        CoreVoiceCodec& owner;
        int input_sample_rate()const{return owner.core_.input_sample_rate();}
        int output_sample_rate()const{return owner.core_.output_sample_rate();}
        int input_channels()const{return owner.core_.input_channels();}
        int output_channels()const{return owner.core_.output_channels();}
        bool MuseInputDataChecked(const AudioLease& l,std::vector<int16_t>& data){return owner.Allowed(voice_codec_prep::Operation::Input)&&owner.core_.MuseInputDataChecked(l,data)&&owner.Allowed(voice_codec_prep::Operation::Input);}
        bool MuseOutputDataChecked(const AudioLease& l,std::vector<int16_t>& data){return owner.Allowed(voice_codec_prep::Operation::Output)&&owner.core_.MuseOutputDataChecked(l,data)&&owner.Allowed(voice_codec_prep::Operation::Output);}
    } checked_{*this};
    PcmAdapter<CheckedCore> pcm_{checked_};
    AudioLease lease_{};Directions selected_{};Gate gate_{};muse_native_review::Clock* clock_=nullptr;
    uint64_t generation_=0;int64_t expires_=0,last_=0,last_wall_=0;
    bool spent_=false,bound_=false,busy_=false,failed_=false,joined_=false;
    bool input_known_=true,output_known_=true,mic_open_=false,speaker_open_=false;
    bool input_stopped_=false,output_stopped_=false;
    std::array<int16_t,832> carry_{};std::array<int16_t,512> converted_{};size_t pending_=0;
    struct Busy{bool& flag;explicit Busy(bool& b):flag(b){flag=true;}~Busy(){flag=false;}};
    static void Zero(int16_t* p,size_t n){if(p){volatile int16_t* v=p;while(n--)*v++=0;}}
    void Wipe(){Zero(carry_.data(),carry_.size());Zero(converted_.data(),converted_.size());pending_=0;}
    bool Same(uint64_t g)const{return bound_&&g&&g==generation_&&CommonAudioAuthority().Holds(lease_);}
    bool Fail(){failed_=true;Wipe();if(lease_)RevokeAudio(lease_);return false;}
    bool Allowed(voice_codec_prep::Operation operation){
        if(failed_||!clock_||!gate_||!lease_||!CommonAudioAuthority().Current(lease_))return false;
        const auto gate=gate_;muse_native_review::Stamp before{},after{};
        if(!clock_->Sample(before)||before.mono_us<last_||before.mono_us<0||before.wall_ms<last_wall_||before.mono_us>=expires_||expires_-before.mono_us>120000000||
           !gate(generation_,before.mono_us,operation)||!clock_->Sample(after)||after.mono_us<before.mono_us||after.wall_ms<before.wall_ms||after.mono_us>=expires_||
           !gate(generation_,after.mono_us,operation)||!CommonAudioAuthority().Current(lease_))return false;
        last_=after.mono_us;last_wall_=after.wall_ms;return true;
    }
public:
    explicit CoreVoiceCodec(Core& core):core_(core){}
    CoreVoiceCodec(const CoreVoiceCodec&)=delete;CoreVoiceCodec& operator=(const CoreVoiceCodec&)=delete;
    ~CoreVoiceCodec(){assert(!bound_);Wipe();}
    bool Reserve(uint64_t)override{return false;}
    bool ReserveExact(uint64_t g,const AudioLease& resource,Directions selected,int64_t expires,muse_native_review::Clock* clock,Gate gate)override{
        if(spent_||bound_||busy_||!g||!clock||!gate||(!selected.microphone&&!selected.output)||
           resource.owner()!=AudioOwner::Muse||resource.purpose()!=AudioPurpose::MusePushToTalk||resource.epoch()!=g||!CommonAudioAuthority().Current(resource))return false;
        Busy busy(busy_);lease_=resource;generation_=g;selected_=selected;expires_=expires;clock_=clock;gate_=std::move(gate);
        if(!Allowed(voice_codec_prep::Operation::Bind)){lease_={};gate_={};clock_=nullptr;return false;}
        spent_=true;
        if(!pcm_.Prepare(resource,selected)||!Allowed(voice_codec_prep::Operation::Bind)||!core_.MuseCodecAcquire(resource)){
            if(pcm_.HasLease())pcm_.Close(resource);
            lease_={};gate_={};clock_=nullptr;return false;
        }
        bound_=true;return true; // no hardware open/read/write or logical-lease release
    }
    voice_codec_prep::Observation Observe(uint64_t g)override{
        if(!Same(g)||busy_)return {};
        return {input_known_&&output_known_,mic_open_,speaker_open_,false,false}; // checked codec state, never actual capture/playback evidence
    }
    bool EnableMicrophone(uint64_t g)override{
        if(!Same(g)){return false;}
        if(busy_){return Fail();}
        Busy busy(busy_);
        if(!selected_.microphone||!Allowed(voice_codec_prep::Operation::Input))return Fail();
        input_known_=false;input_stopped_=output_stopped_=false;
        if(!core_.MuseEnableInputChecked(lease_,true)||!Allowed(voice_codec_prep::Operation::Input))return Fail();
        input_known_=mic_open_=true;joined_=false;return true;
    }
    bool EnableOutput(uint64_t g)override{
        if(!Same(g)){return false;}
        if(busy_){return Fail();}
        Busy busy(busy_);
        if(!selected_.output||!Allowed(voice_codec_prep::Operation::Output))return Fail();
        output_known_=false;output_stopped_=false;
        if(!core_.MuseEnableOutputChecked(lease_,true)||!Allowed(voice_codec_prep::Operation::Output))return Fail();
        output_known_=speaker_open_=true;joined_=false;return true;
    }
    voice_codec_prep::ReadResult ReadFrame(uint64_t g,int16_t* out,size_t capacity,size_t& written)override{
        using Result=voice_codec_prep::ReadResult;written=0;Zero(out,std::min(capacity,size_t(320)));
        if(!Same(g))return Result::Failed;
        if(busy_){Fail();return Result::Failed;}Busy busy(busy_);
        if(!out||capacity!=320||!mic_open_||!input_known_||!Allowed(voice_codec_prep::Operation::Input)){Fail();return Result::Failed;}
        if(pending_<320){size_t count=0;
            if(!pcm_.Read16k(lease_,converted_.data(),converted_.size(),count)||!Allowed(voice_codec_prep::Operation::Input)||count>carry_.size()-pending_){Fail();return Result::Failed;}
            std::copy_n(converted_.data(),count,carry_.data()+pending_);pending_+=count;Zero(converted_.data(),converted_.size());
        }
        if(pending_<320)return Result::Pending; // at most one physical read per call
        std::copy_n(carry_.data(),320,out);pending_-=320;std::move(carry_.begin()+320,carry_.begin()+320+pending_,carry_.begin());Zero(carry_.data()+pending_,carry_.size()-pending_);written=320;return Result::Complete;
    }
    size_t Read16k(uint64_t g,int16_t* out,size_t cap)override{size_t count=0;return ReadFrame(g,out,cap,count)==voice_codec_prep::ReadResult::Complete?count:0;}
    size_t Write16k(uint64_t g,const int16_t* in,size_t count)override{
        if(!Same(g)){return 0;}
        if(busy_){Fail();return 0;}
        Busy busy(busy_);
        if(!in||!count||count>1282||!speaker_open_||!output_known_||!Allowed(voice_codec_prep::Operation::Output)){Fail();return 0;}
        for(size_t offset=0;offset<count;){const auto chunk=std::min(size_t(320),count-offset);size_t consumed=0;
            if(!Allowed(voice_codec_prep::Operation::Output)||!pcm_.Write16k(lease_,in+offset,chunk,consumed)||consumed!=chunk||!Allowed(voice_codec_prep::Operation::Output)){Fail();return 0;}
            offset+=chunk;
        }
        return count; // any uncertain write revokes; caller must stop, never replay
    }
    bool StopMicrophone(uint64_t g)override{
        if(!Same(g)){return false;}
        if(busy_){return Fail();}
        Busy busy(busy_);Wipe();
        const bool stopped=core_.MuseStopInputChecked(lease_);
        const bool reset=!selected_.microphone||!pcm_.HasLease()||pcm_.ResetDsp(lease_,{true,false});
        input_known_=input_stopped_=stopped&&reset;if(input_known_)mic_open_=false;
        return input_known_?true:Fail();
    }
    bool StopOutput(uint64_t g)override{
        if(!Same(g)){return false;}
        if(busy_){return Fail();}
        Busy busy(busy_);
        const bool stopped=core_.MuseStopOutputChecked(lease_);
        const bool reset=!selected_.output||!pcm_.HasLease()||pcm_.ResetDsp(lease_,{false,true});
        output_known_=output_stopped_=stopped&&reset;if(output_known_)speaker_open_=false;
        return output_known_?true:Fail();
    }
    bool FlushOutput(uint64_t,uint64_t)override{return false;} // software reset/stop/DMA sent are not physical discard/flush proof
    bool Join(uint64_t g)override{
        if(!Same(g)||busy_||!input_known_||!output_known_||!input_stopped_||!output_stopped_||mic_open_||speaker_open_)return false;
        Wipe();joined_=true;return true; // synchronous codec owns no task; not transport/UI worker join
    }
    bool Release(uint64_t g)override{
        if(!Same(g)||busy_||!joined_||!input_known_||!output_known_||!input_stopped_||!output_stopped_||mic_open_||speaker_open_)return false;
        Busy busy(busy_);Wipe();
        if(pcm_.HasLease()&&!pcm_.Close(lease_))return Fail();
        if(!core_.MuseCodecRelease(lease_))return Fail();
        bound_=false;gate_={};clock_=nullptr;lease_={};return true; // original logical lease remains Owner's obligation
    }
};
}
#endif
