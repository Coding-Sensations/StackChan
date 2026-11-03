#pragma once
#include "usb_pcm_worker.h"
#include <companion/pair_ui_action.hpp>
namespace direct_usb_pcm {
// Production source binder, not a USB opener or credentials issuer. The owner
// supplies its actual existing channel identity; no default channel or selection.
enum class LocalConsentIntent {MatchOnly,MatchAndListen};
class UsbConsentIssuer final:public SessionAuthority {
 companion::NumericPairingContext* pair_=nullptr;companion::PairUiAction click_;
 Identity selected_;std::string code_,stamp_;const void* channel_=nullptr;
 std::function<const void*()> current_channel_;LocalConsentIntent intent_=LocalConsentIntent::MatchOnly;bool listen_=false,confirmed_=false,staged_=false,taken_=false;
 bool Matches(uint64_t now)const{
  return pair_&&current_channel_&&channel_&&current_channel_()==channel_&&pair_->Active(now)&&pair_->AudioRequested()&&now<selected_.deadline_ms&&pair_->Request()==selected_.request&&pair_->DeviceNonce()==selected_.device_nonce&&pair_->HostNonce()==selected_.host_nonce&&pair_->Boot()==selected_.boot;
 }
public:
 explicit UsbConsentIssuer(std::function<const void*()> channel={}):current_channel_(std::move(channel)){}
 void Cancel(){click_.Reset();pair_=nullptr;selected_={};code_.clear();stamp_.clear();channel_=nullptr;listen_=confirmed_=staged_=taken_=false;intent_=LocalConsentIntent::MatchOnly;}
 // Trusted profile/bundle cutoff is supplied by the controller, not a peer
 // frame. The60s active ceiling starts only at first actual direction start.
 bool Stage(companion::NumericPairingContext& pair,const std::string& device,uint32_t epoch,uint16_t stream,uint64_t trial_deadline,const void* channel,uint64_t now){
  Cancel();if(!channel||!current_channel_||current_channel_()!=channel||!epoch||!stream||device.empty()||device.size()>100||!pair.Active(now)||!pair.AudioRequested()||pair.Granted()||pair.Code().size()!=6||trial_deadline<=now)return false;
  for(auto c:device)if(c<32||c==127)return false;
  pair_=&pair;selected_={pair.Request(),pair.DeviceNonce(),pair.HostNonce(),device,pair.Boot(),epoch,stream,trial_deadline};code_=pair.Code();channel_=channel;
  stamp_=selected_.request+"|usb-pcm-v2|"+device+"|"+std::to_string(epoch)+"|"+std::to_string(stream)+"|"+selected_.device_nonce+"|"+selected_.host_nonce+"|"+std::to_string(selected_.boot);
  click_.Render(code_,stamp_,true);return true;
 }
 // Wire to the explicit matching-code LVGL callback, never a received bool.
 void Click(LocalConsentIntent intent=LocalConsentIntent::MatchOnly){intent_=intent;click_.Click();}
 bool ClickCaptured(const std::string& code,const std::string& stamp,LocalConsentIntent intent){if(code!=code_||stamp!=stamp_||!pair_)return false;Click(intent);return true;}
 bool Confirm(uint64_t now){if(!Matches(now)||pair_->Code()!=code_||confirmed_||click_.Take(code_,stamp_,true)!=companion::PairUiAction::Action::Confirm)return false;if(intent_!=LocalConsentIntent::MatchAndListen)return false;confirmed_=pair_->Confirm(now);listen_=confirmed_;return confirmed_;}
 bool Staged(const Identity& current,const void* channel,uint64_t now){
  if(!Matches(now)||!listen_||!confirmed_||!pair_->Granted()||!pair_->Confirmed()||channel!=channel_||current.request!=selected_.request||current.device_nonce!=selected_.device_nonce||current.host_nonce!=selected_.host_nonce||current.device!=selected_.device||current.boot!=selected_.boot||current.epoch!=selected_.epoch||current.stream!=selected_.stream||current.deadline_ms!=selected_.deadline_ms)return false;
  staged_=true;return true;
 }
 std::unique_ptr<Admission> Take(uint64_t now)override{
  if(taken_||!listen_||!confirmed_||!staged_||!Matches(now)||!pair_->Granted()||!pair_->Confirmed())return {};
  auto result=Seal(*pair_,selected_.device,selected_.epoch,selected_.stream,std::min(selected_.deadline_ms,pair_->GrantDeadline()),now);if(result)taken_=true;return result;
 }
 bool Current(const Admission& token,uint64_t now)const override{
  const auto& id=token.Bound();return token.Issuer()==this&&taken_&&listen_&&confirmed_&&staged_&&Matches(now)&&pair_->Granted()&&pair_->Confirmed()&&id.request==selected_.request&&id.device_nonce==selected_.device_nonce&&id.host_nonce==selected_.host_nonce&&id.device==selected_.device&&id.boot==selected_.boot&&id.epoch==selected_.epoch&&id.stream==selected_.stream&&id.deadline_ms==std::min(selected_.deadline_ms,pair_->GrantDeadline());
 }
 const Identity& Selected()const{return selected_;}const std::string& DisplayedCode()const{return code_;}const std::string& DisplayStamp()const{return stamp_;}
};
}
