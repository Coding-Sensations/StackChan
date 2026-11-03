#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <atomic>
namespace usb_composite_v2 {
class Reports {
 std::array<uint8_t,8> keys_{};bool mounted_=false,suspended_=false,armed_=false,release_=true,inflight_=false;
 uint16_t active_=0,panic_=0;bool second_cc_=false;
public:
 // Sink calls are nonblocking queue attempts, not physical-delivery attestations.
 template<class Sink> void Mounted(Sink& sink){mounted_=true;suspended_=false;armed_=false;keys_={};release_=true;inflight_=false;sink.LocalSilence();}
 template<class Sink> void Lost(Sink& sink,bool suspend){suspended_=suspend;mounted_=suspend;armed_=false;keys_={};release_=true;inflight_=false;panic_|=active_;active_=0;second_cc_=false;sink.LocalSilence();}
 template<class Sink> void Resume(Sink& sink){mounted_=true;suspended_=false;armed_=false;keys_={};release_=true;inflight_=false;sink.LocalSilence();}
 bool ArmKeyboard(bool physical){if(!physical||!mounted_||suspended_||release_||inflight_)return false;armed_=true;return true;}
 bool Press(uint8_t modifiers,const std::array<uint8_t,6>& keys){
  if(!armed_||!mounted_||suspended_||release_||inflight_)return false;
  for(size_t i=0;i<keys.size();++i){if(keys[i]&& (keys[i]<4||keys[i]>0x65))return false;for(size_t j=0;j<i;++j)if(keys[i]&&keys[i]==keys[j])return false;}
  keys_[0]=modifiers;keys_[1]=0;for(size_t i=0;i<6;++i)keys_[i+2]=keys[i];return true;
 }
 void Release(){keys_={};release_=true;armed_=false;}
 void HidComplete(){inflight_=false;}
 std::array<uint8_t,8> InputReport()const{return mounted_&&!suspended_?keys_:std::array<uint8_t,8>{};}
 uint8_t leds=0;
 bool SetLeds(const uint8_t* data,size_t size){if(!data||size!=1||data[0]&0xe0)return false;leds=data[0];return true;}
 template<class Sink> bool MidiInput(Sink& sink,const std::array<uint8_t,4>& p){
  if(!mounted_||suspended_||p[0]>>4)return false;
  auto kind=p[1]&0xf0;auto channel=p[1]&0x0f;auto cin=p[0]&0x0f;
  if((kind!=0x80&&kind!=0x90&&kind!=0xb0)||cin!=(kind>>4)||p[2]>127||p[3]>127)return false;
  if(kind==0xb0){if(p[2]!=120&&p[2]!=123)return false;sink.LocalSilence();return true;}
  return sink.LocalNote(channel,p[2],kind==0x80?0:p[3]); // Never starts the audio owner.
 }
 template<class Sink> bool MidiOutput(Sink& sink,uint8_t channel,uint8_t note,uint8_t velocity){
  if(channel>15||note>127||velocity>127||!mounted_||suspended_||panic_)return false;
  std::array<uint8_t,4> p={uint8_t(velocity?9:8),uint8_t((velocity?0x90:0x80)|channel),note,velocity};
  if(!sink.Midi(p))return false;
  if(velocity)active_|=uint16_t(1u<<channel); // Panic custody retained until explicitly requested.
  return true;
 }
 void AllNotesOff(){panic_|=active_;active_=0;if(!panic_)panic_=0xffff;second_cc_=false;}
 template<class Sink> bool Flush(Sink& sink){
  if(!mounted_||suspended_)return false;
  if(release_){if(inflight_||!sink.Hid(std::array<uint8_t,8>{}))return false;release_=false;inflight_=true;}
  while(panic_){unsigned ch=0;while(!(panic_&(1u<<ch)))++ch;
   std::array<uint8_t,4> p={0x0b,uint8_t(0xb0|ch),uint8_t(second_cc_?123:120),0};
   if(!sink.Midi(p))return false;
   if(second_cc_){panic_&=uint16_t(~(1u<<ch));second_cc_=false;}else second_cc_=true;
  }
  return !inflight_;
 }
 template<class Sink> bool SendKeys(Sink& sink){if(!armed_||release_||inflight_||!mounted_||suspended_)return false;if(!sink.Hid(keys_))return false;inflight_=true;return true;}
 bool Mounted()const{return mounted_;}
 bool Suspended()const{return suspended_;}
 bool CanOperate()const{return mounted_&&!suspended_&&!release_&&!inflight_;}
};
enum class Mode { Legacy,Composite,CleanupPending,RebootRequired };
enum class Failure { None,NotSelected,MissingDependency,LegacyBusy,Claimed,Phy,Install,Close,Identity,Reboot };
inline std::atomic<bool> composite_claim{false};
// Boot-only selection. No running Serial/JTAG -> OTG hot migration.
// Backend must report actual joined TinyUSB shutdown before PHY deletion.
template<class Backend> class BootMode {
 Backend& io_;bool owned_=false,phy_=false,device_=false;Mode mode_=Mode::Legacy;Failure failure_=Failure::None;
public:
 explicit BootMode(Backend& io):io_(io){}
 Mode Current()const{return mode_;}Failure Error()const{return failure_;}
 bool Start(bool explicitly_selected,uint16_t vid,uint16_t pid){
  if(owned_)return false;
  if(mode_==Mode::RebootRequired){failure_=Failure::Reboot;return false;}
  if(!explicitly_selected){failure_=Failure::NotSelected;return false;}
  if(!vid||!pid||vid==0xffff||pid==0xffff||(vid==0x303a&&pid==0x1001)){failure_=Failure::Identity;return false;}
  if(!io_.Available()){failure_=Failure::MissingDependency;return false;}
  if(!io_.LegacyQuiescent()){failure_=Failure::LegacyBusy;return false;}
  bool expected=false;if(!composite_claim.compare_exchange_strong(expected,true)){failure_=Failure::Claimed;return false;}
  owned_=true;mode_=Mode::CleanupPending;
  if(!io_.OpenPhy()){failure_=Failure::Phy;Close();return false;}
  phy_=true;
  device_=true; // Installation may fail after partially starting a task.
  if(!io_.Install(vid,pid)){failure_=Failure::Install;Close();return false;}
  mode_=Mode::Composite;failure_=Failure::None;return true;
 }
 bool Close(){
  if(!owned_)return true;
  mode_=Mode::CleanupPending;
  if(device_){if(!io_.StopJoined()){failure_=Failure::Close;return false;}device_=false;}
  if(phy_){if(!io_.DeletePhy()){failure_=Failure::Close;return false;}phy_=false;}
  composite_claim=false;owned_=false;mode_=Mode::RebootRequired;return true;
 }
 bool SwitchRunningToLegacy(){return false;} // Physical reboot/recovery, never silent live takeover.
};
}
