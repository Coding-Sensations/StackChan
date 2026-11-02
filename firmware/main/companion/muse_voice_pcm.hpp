// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_direct_audio.hpp"
#include "esp_ae_rate_cvt.h"
#include <algorithm>
#include <array>
#include <vector>
namespace companion::muse {
// Serialized by the caller's retained worker. Owns DSP and scratch PCM only;
// never enables/stops hardware, issues permissions or releases audio custody.
template<class Core>
class PcmAdapter {
    Core& core_;
    AudioLease lease_{};
    Directions selected_{};
    esp_ae_rate_cvt_handle_t capture_=nullptr,playback_=nullptr;
    bool ready_=false;
    uint64_t last_generation_=0;
    std::vector<int16_t> raw_input_=std::vector<int16_t>(960,0);
    std::vector<int16_t> raw_output_=std::vector<int16_t>(768,0);
    std::array<int16_t,480> mono_{};
    std::array<int16_t,320> source_{};
    static void Zero(int16_t* p,size_t n){if(p){volatile int16_t* v=p;while(n--)*v++=0;}}
    void Wipe(){raw_input_.resize(960);raw_output_.resize(768);Zero(raw_input_.data(),raw_input_.size());Zero(raw_output_.data(),raw_output_.size());Zero(mono_.data(),mono_.size());Zero(source_.data(),source_.size());}
    bool Match(const AudioLease& l)const{
        return lease_&&l.owner()==AudioOwner::Muse&&l.purpose()==AudioPurpose::MusePushToTalk&&
            l.generation()==lease_.generation()&&l.epoch()==lease_.epoch()&&CommonAudioAuthority().Holds(l);
    }
    bool Current(const AudioLease& l)const{return ready_&&Match(l)&&CommonAudioAuthority().Current(l);}
    bool Fail(){ready_=false;Wipe();if(lease_)RevokeAudio(lease_);return false;}
    bool Format()const{return core_.input_sample_rate()==24000&&core_.output_sample_rate()==24000&&
        (core_.input_channels()==1||core_.input_channels()==2)&&core_.output_channels()==1;}
    bool Open(esp_ae_rate_cvt_handle_t& handle,int from,int to){
        esp_ae_rate_cvt_cfg_t cfg{};cfg.src_rate=from;cfg.dest_rate=to;cfg.channel=1;
        cfg.bits_per_sample=16;cfg.complexity=2;cfg.perf_type=ESP_AE_RATE_CVT_PERF_TYPE_MEMORY;
        return esp_ae_rate_cvt_open(&cfg,&handle)==ESP_AE_ERR_OK&&handle;
    }
    void CloseDsp(){if(capture_){esp_ae_rate_cvt_close(capture_);capture_=nullptr;}if(playback_){esp_ae_rate_cvt_close(playback_);playback_=nullptr;}}
public:
    static constexpr size_t CaptureCapacity=512,PlaybackFrames=320;
    explicit PcmAdapter(Core& core):core_(core){}
    PcmAdapter(const PcmAdapter&)=delete;
    PcmAdapter& operator=(const PcmAdapter&)=delete;
    ~PcmAdapter(){Wipe();CloseDsp();} // no hardware stop/release receipt implied
    bool Prepare(const AudioLease& l,Directions selected){
        if(lease_||!l.generation()||l.generation()<=last_generation_||(!selected.microphone&&!selected.output)||!Format()||
           l.owner()!=AudioOwner::Muse||l.purpose()!=AudioPurpose::MusePushToTalk||!CommonAudioAuthority().Current(l))return false;
        lease_=l;selected_=selected;last_generation_=l.generation();
        if(selected.microphone&&(!Open(capture_,24000,16000)||!CommonAudioAuthority().Current(l)))return Fail();
        if(selected.output&&(!Open(playback_,16000,24000)||!CommonAudioAuthority().Current(l)))return Fail();
        ready_=true;return true;
    }
    bool Read16k(const AudioLease& l,int16_t* out,size_t capacity,size_t& written){
        written=0;Zero(out,std::min(capacity,CaptureCapacity));
        if(!Match(l))return false;
        if(!out||capacity!=CaptureCapacity||!selected_.microphone||!capture_||!Current(l)||!Format())return Fail();
        raw_input_.resize(480*static_cast<size_t>(core_.input_channels()));
        if(!core_.MuseInputDataChecked(l,raw_input_)||!Current(l)||raw_input_.size()!=480*static_cast<size_t>(core_.input_channels()))return Fail();
        for(size_t i=0;i<480;++i)mono_[i]=raw_input_[i*static_cast<size_t>(core_.input_channels())];
        uint32_t maximum=0;
        if(esp_ae_rate_cvt_get_max_out_sample_num(capture_,480,&maximum)!=ESP_AE_ERR_OK||!Current(l)||!maximum||maximum>capacity)return Fail();
        uint32_t count=static_cast<uint32_t>(capacity);
        const auto status=esp_ae_rate_cvt_process(capture_,mono_.data(),480,out,&count);
        if(status!=ESP_AE_ERR_OK||count>capacity||!Current(l)){Zero(out,capacity);return Fail();}
        written=count;Zero(out+count,capacity-count);Wipe();return true;
    }
    bool Write16k(const AudioLease& l,const int16_t* in,size_t count,size_t& consumed){
        consumed=0;
        if(!Match(l))return false;
        if(!in||!count||count>PlaybackFrames||!selected_.output||!playback_||!Current(l)||!Format())return Fail();
        std::copy_n(in,count,source_.data());raw_output_.resize(768);
        uint32_t maximum=0;
        if(esp_ae_rate_cvt_get_max_out_sample_num(playback_,static_cast<uint32_t>(count),&maximum)!=ESP_AE_ERR_OK||!Current(l)||!maximum||maximum>raw_output_.size())return Fail();
        uint32_t output=static_cast<uint32_t>(raw_output_.size());
        const auto status=esp_ae_rate_cvt_process(playback_,source_.data(),static_cast<uint32_t>(count),raw_output_.data(),&output);
        if(status!=ESP_AE_ERR_OK||output>raw_output_.size()||!Current(l))return Fail();
        raw_output_.resize(output);
        if(output&&(!core_.MuseOutputDataChecked(l,raw_output_)||!Current(l)))return Fail();
        consumed=count;Wipe();return true;
    }
    bool Close(const AudioLease& l){
        if(!Match(l))return false;
        ready_=false;Wipe();CloseDsp();lease_={};selected_={};return true;
    }
};
}
#endif
