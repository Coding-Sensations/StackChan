#pragma once
#include "canonical_frontend_bridge.hpp"
#include "model_adapter.hpp"
namespace companion::wake_runtime {
inline constexpr bool kNativePortsLiveAvailable=false;
class CanonicalFrontPort {
    mira_native_frontend::Bridge bridge_;
public:
    bool Configure()noexcept{return bridge_.Configure();}
    bool Reset()noexcept{return bridge_.Reset();}
    bool Close()noexcept{bridge_.Close();return !bridge_.Configured()&&bridge_.Samples()==0&&bridge_.Frames()==0;}
    template<class F>bool Feed(const int16_t* pcm,size_t count,F&& callback)noexcept{return bridge_.Feed(pcm,count,static_cast<F&&>(callback));}
    bool Pack(const std::array<uint16_t,40>& raw,std::array<int8_t,120>& features)noexcept{return bridge_.Pack(raw,features);}
};
// Exclusive CPU adapter storage, placed by owner off the small firmware task stack.
// Asset is the frozen audited offline model; no download/install or selection here.
class FixedModelPort {
    mira_model::ModelAdapter adapter_;
    const uint8_t* asset_=nullptr;size_t bytes_=0;uint64_t generation_=0,deadline_=0;
public:
    FixedModelPort(mira_model::ModelAdapter::Clock clock,void* context,const uint8_t* asset=nullptr,size_t bytes=0)noexcept:adapter_(clock,context),asset_(asset),bytes_(bytes){}
    bool Configure(uint64_t generation,uint64_t deadline)noexcept{generation_=generation;deadline_=deadline;return adapter_.Configure(asset_,bytes_,generation,deadline)==mira_model::Status::Ok;}
    bool Reset(uint64_t generation,uint64_t deadline)noexcept{return generation==generation_&&deadline==deadline_&&adapter_.Reset(generation,deadline)==mira_model::Status::Ok;}
    bool Invoke(const int8_t* features,size_t count,uint64_t generation,uint64_t deadline,uint8_t& raw)noexcept{raw=0;return generation==generation_&&deadline==deadline_&&adapter_.Invoke(features,count,generation,deadline,raw)==mira_model::Status::Ok;}
    bool Close(uint64_t generation,uint64_t deadline)noexcept{
        if(generation_&& (generation!=generation_||deadline!=deadline_))return false;
        adapter_.Close();if(adapter_.Configured()||!adapter_.ArenaZero())return false;generation_=deadline_=0;return true;
    }
};
}
