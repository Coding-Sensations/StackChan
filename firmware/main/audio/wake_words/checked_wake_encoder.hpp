#pragma once
#include "child_audio_lifecycle.hpp"
#include "audio_service.h" // selected SDK Opus configuration and error types
#include <algorithm>
#include <condition_variable>
#include <chrono>
#include <deque>

// Per-object encoder worker owns a moved bounded PCM snapshot. Dynamic task
// storage prevents reuse/free of the legacy static task stack before deletion.
class CheckedWakeEncoder {
    checked_audio_child::Lifecycle& life_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::vector<uint8_t>> packets_;
    std::deque<std::vector<int16_t>> pcm_;
    std::atomic<bool> active_{false},complete_{true};
    void Run(){
        void* encoder=nullptr;esp_opus_enc_config_t cfg=AS_OPUS_ENC_CONFIG();
        bool ok=life_.Current()&&esp_opus_enc_open(&cfg,sizeof(cfg),&encoder)==ESP_AUDIO_ERR_OK&&encoder;
        int frame_bytes=0,out_bytes=0;
        if(ok)ok=esp_opus_enc_get_frame_size(encoder,&frame_bytes,&out_bytes)==ESP_AUDIO_ERR_OK&&frame_bytes>0&&frame_bytes%sizeof(int16_t)==0&&frame_bytes<=8192&&out_bytes>0&&out_bytes<=8192;
        std::vector<int16_t> input;
        if(ok){
            const size_t samples=frame_bytes/sizeof(int16_t);
            for(auto& pcm:pcm_){
                if(!life_.Current())break;
                input.insert(input.end(),pcm.begin(),pcm.end());checked_audio_child::Wipe(pcm);
                while(input.size()>=samples&&life_.Current()){
                    std::vector<uint8_t> encoded(out_bytes,0);
                    esp_audio_enc_in_frame_t in{};esp_audio_enc_out_frame_t out{};
                    in.buffer=reinterpret_cast<uint8_t*>(input.data());in.len=frame_bytes;
                    out.buffer=encoded.data();out.len=out_bytes;
                    const auto result=esp_opus_enc_process(encoder,&in,&out);
                    if(result!=ESP_AUDIO_ERR_OK||out.encoded_bytes>encoded.size()){checked_audio_child::Wipe(encoded);ok=false;break;}
                    encoded.resize(out.encoded_bytes);
                    if(life_.Current()){
                        std::lock_guard<std::mutex> lock(mutex_);
                        if(life_.Current()&&packets_.size()<96)packets_.push_back(std::move(encoded));
                    }
                    checked_audio_child::Wipe(encoded);cv_.notify_all();
                    std::fill_n(input.begin(),samples,0);input.erase(input.begin(),input.begin()+samples);
                }
                if(!ok)break;
            }
        }
        checked_audio_child::Wipe(input);
        for(auto& pcm:pcm_){checked_audio_child::Wipe(pcm);}
        pcm_.clear();
        if(encoder)esp_opus_enc_close(encoder);
        // On failure/stale authorization no packets may escape.
        {std::lock_guard<std::mutex> lock(mutex_);if(!ok||!life_.Current()){for(auto& p:packets_)checked_audio_child::Wipe(p);packets_.clear();}complete_=true;}
        cv_.notify_all();active_=false;
    }
public:
    explicit CheckedWakeEncoder(checked_audio_child::Lifecycle& life):life_(life){}
    ~CheckedWakeEncoder(){configASSERT(!active_);}
    bool Start(std::deque<std::vector<int16_t>> snapshot){
        if(active_||!life_.Current())return false;
        {std::lock_guard<std::mutex> lock(mutex_);for(auto& p:packets_)checked_audio_child::Wipe(p);packets_.clear();pcm_=std::move(snapshot);complete_=false;active_=true;}
        life_.WorkerStarted();
        if(xTaskCreate([](void* p){auto* self=static_cast<CheckedWakeEncoder*>(p);self->Run();self->life_.WorkerFinished();vTaskDelete(nullptr);},"wake_encode",24576,this,2,nullptr)!=pdPASS){
            life_.WorkerFinished();active_=false;complete_=true;Clear();cv_.notify_all();return false;
        }
        return true;
    }
    bool Active()const{return active_.load();}
    void NotifyStop(){cv_.notify_all();}
    void Clear(){std::lock_guard<std::mutex> lock(mutex_);configASSERT(!active_);for(auto& p:packets_)checked_audio_child::Wipe(p);packets_.clear();for(auto& p:pcm_)checked_audio_child::Wipe(p);pcm_.clear();complete_=true;}
    bool Get(std::vector<uint8_t>& out){
        checked_audio_child::Wipe(out);std::unique_lock<std::mutex> lock(mutex_);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(packets_.empty()&&!complete_&&life_.Current()){
            if(cv_.wait_until(lock,std::min(deadline,std::chrono::steady_clock::now()+std::chrono::milliseconds(20)))==std::cv_status::timeout&&std::chrono::steady_clock::now()>=deadline)return false;
        }
        if(!life_.Current()){for(auto& p:packets_)checked_audio_child::Wipe(p);packets_.clear();return false;}
        if(packets_.empty())return false;
        out=std::move(packets_.front());packets_.pop_front();return !out.empty();
    }
};
