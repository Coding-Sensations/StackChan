#pragma once
#include "physical_source_gesture.hpp"
#include "desk_codec_adapter.hpp"
#include "detached_cpu_endpoint_receipt.hpp"
#include "psram_endpoint_factory.hpp"
#include "wake_model_asset_provider.hpp"
#include "desk_audio_owner_bridge.hpp"
#include <audio/audio_codec.h>
namespace companion::wake_runtime {
inline constexpr bool kLocalWakeHardwareRouteReady=false;
// Source-owned IO owner, separate from CPU endpoint. Entire Begin/Read/Close
// lifetime is serialized on its worker; controller Stop joins that worker.
class LocalWakeHardwareOwner {
    ConsentAuthority& authority_;PhysicalSourceDispatcher& dispatcher_;
    ClockFn clock_;void* clock_context_;AudioLease lease_;ExplicitLocalConsent proof_;
    desk::AudioBinding binding_{};uint32_t media_=0;bool cpu_closed_=false,busy_=false;
    FactoryHandle factory_;BorrowedCpuEndpoint* endpoint_=nullptr;
    desk::EspMonoResampler resampler_;
    desk::BorrowedCodecInput<AudioCodec,LocalWakeHardwareOwner> reader_;
    template<class,class,class>friend class ::companion::desk::BorrowedCodecInput;
    bool AcquireNoneToMira(desk::AudioBinding binding,desk::InputPurpose purpose){return Current(binding,purpose);}
    bool HoldsMiraLease(desk::AudioBinding binding){return binding==binding_&&CommonAudioAuthority().Holds(lease_);}
    bool Current(desk::AudioBinding binding,desk::InputPurpose purpose){return binding==binding_&&purpose==desk::InputPurpose::LocalWake&&authority_.InputCurrent(proof_,lease_,media_,IntentPurpose::LocalWake)&&CommonAudioAuthority().Current(lease_);}
    bool RevokeMira(desk::AudioBinding binding){return HoldsMiraLease(binding)&&RevokeAudio(lease_);}
    bool ReleaseMira(desk::AudioBinding binding){return cpu_closed_&&HoldsMiraLease(binding)&&ReleaseAudio(lease_,true);}
public:
    LocalWakeHardwareOwner(AudioCodec& codec,ConsentAuthority& authority,PhysicalSourceDispatcher& dispatcher,ClockFn clock,void* context):authority_(authority),dispatcher_(dispatcher),clock_(clock),clock_context_(context),reader_(codec,*this,resampler_){}
    LocalWakeHardwareOwner(const LocalWakeHardwareOwner&)=delete;
    bool Begin(const CapturedPhysicalGesture& gesture){
        const auto asset=QualifiedAssetView();
        if(!kLocalWakeHardwareRouteReady||!desk::kAllAudioConsumersCovered||!desk::kExactLocalWakeModelVerified||!kLiveBorrowedProcessingAvailable||!asset.qualification||!asset.bytes||!asset.size||lease_||busy_||factory_.Endpoint())return false;
        if(!dispatcher_.Validate(gesture,0,gesture.media_,IntentPurpose::LocalWake))return false;
        if(!AcquireAudio(AudioOwner::Mira,AudioPurpose::LocalWake,gesture.media_,lease_))return false;
        binding_={lease_.generation(),lease_.epoch()};media_=gesture.media_;cpu_closed_=false;
        if(!dispatcher_.IssueBound(gesture,lease_,0,media_,IntentPurpose::LocalWake,proof_)||
           !factory_.CreateQualified(lease_.generation(),authority_,clock_,clock_context_,asset.bytes,asset.size)){Close();return false;}
        endpoint_=factory_.Endpoint();
        if(!endpoint_||!endpoint_->AcceptPhysicalProof(lease_,media_,proof_)||!reader_.Begin(binding_,desk::InputPurpose::LocalWake)){Close();return false;}
        return true;
    }
    bool ReadOne(){
        if(busy_||!endpoint_||!lease_)return false;
        busy_=true;
        std::array<int16_t,512> pcm{};size_t count=0;
        const bool read=reader_.Read16k(binding_,pcm.data(),pcm.size(),count);
        const bool fed=read&&Current(binding_,desk::InputPurpose::LocalWake)&&endpoint_->Feed16k(lease_,media_,pcm.data(),count)&&Current(binding_,desk::InputPurpose::LocalWake);
        desk::WipeSamples(pcm.data(),pcm.size());busy_=false;return fed;
    }
    bool Close(){
        if(busy_)return false;
        const uint64_t generation=binding_.generation;
        if(lease_){RevokeAudio(lease_);authority_.Cancel(proof_);}
        cpu_closed_=!endpoint_||endpoint_->CloseCpu();
        if(lease_){
            // Input may never have opened; checked caller close still owns IO.
            if(reader_.HasLease()){if(!reader_.Stop(binding_))return false;}
            else if(!cpu_closed_||!ReleaseAudio(lease_,true))return false; // no IO open attempted

        }
        if(!cpu_closed_)return false;
        if(endpoint_){const DetachedCpuEndpointReceipt receipt{endpoint_,generation};endpoint_=nullptr;if(!factory_.DestroyDetached(receipt)){endpoint_=factory_.Endpoint();return false;}}
        binding_={};proof_={};media_=0;return true;
    }
};
}
