// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include <hal/board/cores3_audio_codec.h>
#include <atomic>
#include <cassert>
#include <cstdio>
namespace companion::muse {
// Caller-owned observation only. No device/Core pointer, operation/action API,
// public receipt ingestion, credentials, identity strings or persistent store.
class QuiesceDiagnostic final {
public:
 enum class State:uint8_t{Empty,Recording,Ready};
 enum class Reason:uint8_t{NotObserved,Attempted,BindingUnavailable,Retired,ActiveOperation,AlreadyAttempted,InvalidTicket,PriorDiscard,MissingInterface};
 enum class Recovery:uint8_t{NoObservation,OwnerCleanupStillRequired,RetainedFreshStateRequired,CustodyLost,Unknown};
 struct Projection {
  State state=State::Empty;CoreS3AudioCodec::MuseQuiesceEvidence evidence{};
  Reason reason=Reason::NotObserved;bool held_at_capture=false,retained_at_capture=false,parked_at_capture=false;Recovery recovery=Recovery::NoObservation;
 };
 static constexpr size_t TextCapacity=768;
private:
 std::atomic<State>state_{State::Empty};AudioLease lease_;uint64_t ticket_=0;Projection data_;
 bool Begin(const AudioLease&lease,uint64_t ticket){
  auto empty=State::Empty;if(!state_.compare_exchange_strong(empty,State::Recording))return false;
  lease_=lease;ticket_=ticket;return true;
 }
 void Finish(uint64_t ticket,const CoreS3AudioCodec::MuseQuiesceEvidence&e,Reason reason,bool held,bool retained,bool parked){
  assert(state_.load()==State::Recording&&ticket==ticket_);data_.state=State::Ready;data_.evidence=e;data_.held_at_capture=held;data_.retained_at_capture=retained;data_.parked_at_capture=parked;data_.reason=reason;
  if(reason==Reason::ActiveOperation)data_.recovery=Recovery::NoObservation;
  else if(retained)data_.recovery=held?Recovery::RetainedFreshStateRequired:Recovery::CustodyLost;
  else if(e.outcome==CoreS3AudioCodec::MuseQuiesce::RegistersAndStoppedDma)data_.recovery=held?Recovery::OwnerCleanupStillRequired:Recovery::CustodyLost;
  else if(e.outcome==CoreS3AudioCodec::MuseQuiesce::Unavailable)data_.recovery=Recovery::NoObservation;
  else data_.recovery=Recovery::Unknown;
  state_.store(State::Ready,std::memory_order_release);
 }
 static const char* OutcomeName(CoreS3AudioCodec::MuseQuiesce o){switch(o){case CoreS3AudioCodec::MuseQuiesce::Unavailable:return "unavailable";case CoreS3AudioCodec::MuseQuiesce::Failed:return "failed";case CoreS3AudioCodec::MuseQuiesce::RegistersAndStoppedDma:return "registers-and-stopped-dma";default:return "unknown";}}
 static const char* PhaseName(CoreS3AudioCodec::MuseQuiescePhase p){switch(p){case CoreS3AudioCodec::MuseQuiescePhase::None:return "none";case CoreS3AudioCodec::MuseQuiescePhase::Amplifier:return "amplifier";case CoreS3AudioCodec::MuseQuiescePhase::InputStop:return "input-stop";case CoreS3AudioCodec::MuseQuiescePhase::OutputStop:return "output-stop";case CoreS3AudioCodec::MuseQuiescePhase::DigitalDiscard:return "digital-discard";case CoreS3AudioCodec::MuseQuiescePhase::FinalReadback:return "final-readback";case CoreS3AudioCodec::MuseQuiescePhase::Complete:return "complete";default:return "unknown";}}
 static const char* ReasonName(Reason r){switch(r){case Reason::Attempted:return "attempted";case Reason::BindingUnavailable:return "binding-unavailable";case Reason::Retired:return "retired";case Reason::ActiveOperation:return "active-operation";case Reason::AlreadyAttempted:return "already-attempted";case Reason::InvalidTicket:return "invalid-ticket";case Reason::PriorDiscard:return "prior-discard";case Reason::MissingInterface:return "missing-interface";default:return "not-observed";}}
 static const char* RecoveryName(Recovery r){switch(r){case Recovery::NoObservation:return "no-observation";case Recovery::OwnerCleanupStillRequired:return "owner-cleanup-still-required";case Recovery::RetainedFreshStateRequired:return "retained-fresh-state-required-no-retry";case Recovery::CustodyLost:return "custody-lost-no-action";default:return "unknown-no-action";}}
 friend class ::CoreS3AudioCodec;
public:
 QuiesceDiagnostic()=default;QuiesceDiagnostic(const QuiesceDiagnostic&)=delete;QuiesceDiagnostic&operator=(const QuiesceDiagnostic&)=delete;
 ~QuiesceDiagnostic(){assert(state_.load()!=State::Recording);} // caller retains through synchronous call; no substitute cleanup
 Projection Read()const{
  const auto state=state_.load(std::memory_order_acquire);if(state==State::Ready)return data_;
  Projection empty;empty.state=state;return empty;
 }
 bool Format(char*out,size_t capacity)const{
  if(!out||!capacity)return false;
  out[0]=0;const auto p=Read();const auto&e=p.evidence;const size_t bounded=capacity<TextCapacity?capacity:TextCapacity;
  const int n=std::snprintf(out,bounded,"snapshot-only no-authority physical-flush=unqualified; state=%s source-reason=%s recovery=%s; outcome=%s phase=%s amp-stage=%u amp-fault=%u amp-code=%d amp-mute=%d/%d amp-system=%d/%d stop-code=%d stop-input=%u stop-output=%u dma-phase=%u dma-code=%d dma-loaded=%zu/%zu dma-total=%zu; held-at-capture=%u retained-at-capture=%u parked-at-capture=%u binding-lost=%u",
   p.state==State::Ready?"ready":p.state==State::Recording?"recording":"empty",ReasonName(p.reason),RecoveryName(p.recovery),OutcomeName(e.outcome),PhaseName(e.phase),unsigned(e.amplifier.stage),unsigned(e.amplifier.fault),e.amplifier.status,e.amplifier.mute_before,e.amplifier.mute_after,e.amplifier.system_before,e.amplifier.system_after,e.stop_status,unsigned(e.input_stop_attempts),unsigned(e.output_stop_attempts),unsigned(e.discard_detail.phase),e.discard_detail.status,e.discard_detail.loaded,e.discard_detail.requested,e.discard_detail.total_loaded,unsigned(p.held_at_capture),unsigned(p.retained_at_capture),unsigned(p.parked_at_capture),unsigned(e.binding_lost));
  if(n<0||size_t(n)>=bounded){out[0]=0;return false;}return true;
 }
};
}
#endif
