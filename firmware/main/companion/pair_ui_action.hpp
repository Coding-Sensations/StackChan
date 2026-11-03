#pragma once
#include <atomic>
#include <cstdint>
#include <string>
namespace companion {
// Render/Take belong to the shell. The LVGL callback touches only atomic stamps.
// A physical click authorizes only the code/request/scope actually rendered.
class PairUiAction {
    std::string code_,request_;bool audio_=false;
    uint32_t generation_=1;
    std::atomic<uint32_t> displayed_{1},clicked_{0};
    void Advance(){if(++generation_==0)++generation_;displayed_.store(generation_,std::memory_order_release);}
public:
    enum class Action {None,Pair,Confirm,Stale};
    void Reset(){clicked_.store(0);code_.clear();request_.clear();audio_=false;Advance();}
    void Render(const std::string& code,const std::string& request,bool audio){
        if(code_==code&&request_==request&&audio_==audio)return;
        code_=code;request_=request;audio_=audio;Advance();
    }
    void Click(){clicked_.store(displayed_.load(std::memory_order_acquire),std::memory_order_release);}
    Action Take(const std::string& code,const std::string& request,bool audio){
        uint32_t stamp=clicked_.exchange(0,std::memory_order_acq_rel);
        if(!stamp)return Action::None;
        if(stamp!=generation_||code_!=code||request_!=request||audio_!=audio)return Action::Stale;
        return code_.empty()?Action::Pair:Action::Confirm;
    }
};
}
