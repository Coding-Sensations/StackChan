#pragma once
#include <companion/numeric_pairing.h>
#include <companion/audio_session.hpp>
#include <companion/audio_owner.hpp>
#include <audio/audio_codec.h>
#include <esp_ae_rate_cvt.h>
#include <array>
#include <bitset>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
namespace direct_usb_pcm {
struct Identity {std::string request,device_nonce,host_nonce,device;uint32_t boot=0,epoch=0;uint16_t stream=0;uint64_t deadline_ms=0;};
class SessionAuthority;
// No public constructor, boolean approval or caller-manufactured session.
class Admission final {
 friend class SessionAuthority;Identity identity_;const SessionAuthority* issuer_;explicit Admission(Identity v,const SessionAuthority* issuer):identity_(std::move(v)),issuer_(issuer){}
public:
 Admission(const Admission&)=delete;Admission& operator=(const Admission&)=delete;
 const Identity& Bound()const{return identity_;}
 const SessionAuthority* Issuer()const{return issuer_;}
};
class SessionAuthority {
protected:
 // A future trusted USB owner must additionally bind selected transport/stream,
 // actual matching physical action and staged ACK. This alone is NOT that hookup.
 std::unique_ptr<Admission> Seal(const companion::NumericPairingContext& c,const std::string& device,uint32_t epoch,uint16_t stream,uint64_t deadline,uint64_t now){
  if(device.empty()||!epoch||!stream||!c.Active(now)||!c.Confirmed()||!c.Granted()||!c.AudioRequested()||deadline<=now||deadline>c.GrantDeadline())return {};
  return std::unique_ptr<Admission>(new Admission({c.Request(),c.DeviceNonce(),c.HostNonce(),device,c.Boot(),epoch,stream,deadline},this));
 }
public:
 virtual ~SessionAuthority()=default;
 virtual std::unique_ptr<Admission> Take(uint64_t){return {};}
 virtual bool Current(const Admission&,uint64_t)const{return false;}
};
enum class ClaimResult {Refused,Owned,Uncertain};
class Endpoint {
public:
 virtual ~Endpoint()=default;
 virtual ClaimResult Claim(const Admission&){return ClaimResult::Refused;}
 virtual bool Current(const Admission&)const{return false;}
 virtual bool Close(){return true;}
 virtual bool MicCapacity(size_t)const{return false;}
 virtual bool Mic(const Identity&,uint32_t,const uint8_t*,size_t){return false;}
 virtual bool SpeakerCredit(const Identity&,size_t){return false;}
};
enum class Fault {None,Unavailable,Busy,Format,UsbClaim,Authority,Packet,Queue,Direction,Read,Write,Resampler,UsbSend,Credit,Cleanup};
struct Snapshot {bool selected=false,active=false,playing=false,closing=false,cleanup_failed=false;Fault fault=Fault::None;uint64_t speaker_frames=0,mic_frames=0;unsigned queued=0;};
// All effectful calls belong to one future retained owner worker. Only enqueue
// and atomic RequestStop may be called by its parser/control callbacks.
class Worker {
 struct Speaker {std::array<uint8_t,3840> pcm{};};
 SessionAuthority& authority_;Endpoint& endpoint_;std::function<uint64_t()> now_;
 std::unique_ptr<Admission> admission_;AudioCodec* codec_=nullptr;esp_ae_rate_cvt_handle_t resampler_=nullptr;
 companion::AudioSession session_;bool codec_claim_=false,drain_pending_=false,usb_claim_=false,active_=false,known_=false,playing_=false;
 std::atomic<bool> quit_{false},closed_{true};std::mutex mutex_;std::array<Speaker,2> queue_{};unsigned head_=0,count_=0;
 uint64_t trial_start_=0,trial_deadline_=0;
 uint32_t speaker_sequence_=0,mic_sequence_=0,last_epoch_=0;std::bitset<65536> used_streams_{};
 std::vector<int16_t> hardware_,mono_,converted_,slice_,pending_;Snapshot snapshot_;
 bool Current();void Fail(Fault);bool Cleanup();bool FlushMic();
public:
 Worker(SessionAuthority& a,Endpoint& e,std::function<uint64_t()> now):authority_(a),endpoint_(e),now_(std::move(now)){}
 ~Worker();Worker(const Worker&)=delete;Worker& operator=(const Worker&)=delete;
 bool Select(std::function<AudioCodec*()> resolve);
 bool BeginMic(uint32_t epoch,uint16_t stream);
 bool BeginSpeaker(uint32_t epoch,uint16_t stream);
 bool Playing(uint32_t epoch,uint16_t stream,bool playing);
 bool Heartbeat(uint32_t epoch,uint16_t stream);
 bool CurrentSession(uint32_t epoch,uint16_t stream);
 bool StopMic(uint32_t epoch,uint16_t stream);
 bool FinishSpeaker(uint32_t epoch,uint16_t stream);
 bool Receive(uint16_t stream,uint32_t sequence,uint32_t rate,const uint8_t* pcm,size_t bytes);
 void RequestStop(){quit_=true;}
 bool Step();bool Closed()const{return closed_.load();}
 Snapshot Status();
};
}
