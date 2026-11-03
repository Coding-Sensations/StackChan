// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_device.h"
#include "muse_direct_admission.hpp"
#include <atomic>
#include <memory>
namespace companion::muse {
enum class LocalMode{Pair,Chat};enum class LocalField{None,Token,Vm,Chat,Message};
struct LocalIdentity{std::string node,device,mac,version;};
struct LocalNetwork{std::array<uint8_t,32> ssid{};std::array<uint8_t,6> bssid{};uint64_t epoch=0;bool valid=false;};
class LocalNetworkUse{public:virtual ~LocalNetworkUse()=default;virtual bool Current()=0;};
class LocalPort{
public:
 virtual ~LocalPort()=default;virtual uint64_t Now()=0;virtual bool Foreground()=0;
 virtual bool Identity(LocalIdentity&)=0;virtual bool Network(LocalNetwork&)=0;virtual bool NetworkMatches(const LocalNetwork&)=0;
 virtual std::unique_ptr<LocalNetworkUse> Borrow(const LocalNetwork&)=0;
 virtual bool PairIdle()=0;virtual bool ChatIdle()=0;
 virtual bool PreparePair(const PairingGrant&,std::string_view)=0;virtual bool PrepareChat(const SessionGrant&)=0;
 virtual void Stop(LocalMode)=0;virtual bool Text(const std::string& generation,const std::string& request,const std::string& text)=0;
 virtual bool RandomId(std::string&)=0;
};
struct LocalScope {
 LocalPort& port;LocalMode mode;std::unique_ptr<LocalNetworkUse> network;std::atomic<bool> active{true};uint64_t expires=0;std::string generation;
 LocalScope(LocalPort& p,LocalMode m):port(p),mode(m){}
 bool Current(){return active&&port.Foreground()&&port.Now()<expires;}
 bool NetworkCurrent(){return Current()&&network&&network->Current();}
};
template<size_t N>class LocalText {
 std::array<char,N+1> bytes_{};size_t length_=0;
public:
 ~LocalText(){Clear();}LocalText()=default;LocalText(const LocalText&)=delete;
 bool Append(char c){if(length_==N)return false;bytes_[length_++]=c;bytes_[length_]=0;return true;}
 void Pop(){if(length_)bytes_[--length_]=0;}size_t Size()const{return length_;}
 std::string_view View()const{return {bytes_.data(),length_};}
 void Clear(){for(volatile char& c:bytes_)c=0;length_=0;}
};
// Only this model's staging method can expose the private token to its port.
// Renderer-facing Value(Token) is always empty; no LVGL text area owns it.
class LocalSetupModel {
 LocalPort& port_;LocalMode mode_=LocalMode::Pair;LocalField field_=LocalField::None;
 LocalIdentity identity_;LocalNetwork network_;bool identity_ok_=false;std::array<bool,3> scopes_{};
 uint64_t epoch_=0,input_=0,approval_until_=0,token_until_=0,last_=0;bool broken_=false;
 LocalText<43> token_;LocalText<128> vm_,chat_;LocalText<256> message_;
 std::shared_ptr<LocalScope> scope_;std::string message_generation_;
 bool Time(){const auto now=port_.Now();if(now<last_){broken_=true;CancelScope();token_.Clear();}last_=now;return !broken_&&port_.Foreground();}
 void CancelScope(){if(scope_){scope_->active=false;port_.Stop(scope_->mode);scope_.reset();}message_.Clear();message_generation_.clear();}
 void Changed(){CancelScope();scopes_.fill(false);approval_until_=0;token_.Clear();}
 bool Approved(){return Time()&&identity_ok_&&network_.valid&&scopes_[0]&&scopes_[1]&&scopes_[2]&&port_.Now()<approval_until_&&port_.NetworkMatches(network_);}
 bool Base64(char c)const{return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';}
public:
 explicit LocalSetupModel(LocalPort& port):port_(port){}~LocalSetupModel(){Close();}
 LocalSetupModel(const LocalSetupModel&)=delete;
 uint64_t Begin(){Close();broken_=false;last_=port_.Now();return ++epoch_;}
 void Close(){CancelScope();token_.Clear();vm_.Clear();chat_.Clear();message_.Clear();identity_={};network_={};identity_ok_=false;scopes_.fill(false);field_=LocalField::None;++input_;++epoch_;}
 uint64_t Epoch()const{return epoch_;}uint64_t InputEpoch()const{return input_;}LocalMode Mode()const{return mode_;}LocalField Field()const{return field_;}
 bool Select(uint64_t captured,LocalMode mode){if(captured!=epoch_||!Time())return false;Changed();mode_=mode;field_=LocalField::None;++input_;return true;}
 bool ReviewIdentity(uint64_t captured){if(captured!=epoch_||!Time())return false;Changed();identity_={};identity_ok_=port_.Identity(identity_)&&Id(identity_.node)&&Id(identity_.device)&&Id(identity_.mac,17)&&Id(identity_.version,32);return identity_ok_;}
 bool ReviewNetwork(uint64_t captured){if(captured!=epoch_||!Time())return false;Changed();network_={};return port_.Network(network_)&&network_.valid;}
 const LocalIdentity& Identity()const{return identity_;}const LocalNetwork& Network()const{return network_;}
 bool Allow(uint64_t captured,unsigned index){if(captured!=epoch_||index>2||!Time()||!identity_ok_||!network_.valid||!port_.NetworkMatches(network_))return false;CancelScope();scopes_[index]=!scopes_[index];approval_until_=port_.Now()+300000;return true;}
 bool Allowed(unsigned index)const{return index<3&&scopes_[index];}
 bool Edit(uint64_t captured,LocalField field){
  if(captured!=epoch_||!Time())return false;
  if(field==LocalField::Token){if(mode_!=LocalMode::Pair||!Approved()||!port_.PairIdle()||!port_.ChatIdle())return false;token_.Clear();token_until_=port_.Now()+300000;}
  else if(field==LocalField::Vm||field==LocalField::Chat){if(mode_!=LocalMode::Chat)return false;Changed();if(field==LocalField::Vm)vm_.Clear();else chat_.Clear();}
  else if(field==LocalField::Message){if(!scope_||scope_->mode!=LocalMode::Chat||!scope_->Current())return false;message_.Clear();message_generation_=scope_->generation;}
  else return false;
  field_=field;++input_;return true;
 }
 bool Append(uint64_t captured_input,char c){
  if(captured_input!=input_||!Time())return false;
  if(field_==LocalField::Token)return port_.Now()<token_until_&&Approved()&&Base64(c)&&token_.Append(c);
  if(field_==LocalField::Vm||field_==LocalField::Chat){if(!Id(std::string_view(&c,1),1))return false;return field_==LocalField::Vm?vm_.Append(c):chat_.Append(c);}
  return field_==LocalField::Message&&scope_&&scope_->Current()&&c>=32&&c<=126&&message_.Append(c);
 }
 bool Pop(uint64_t captured_input){if(captured_input!=input_||!Time())return false;if(field_==LocalField::Token)token_.Pop();else if(field_==LocalField::Vm)vm_.Pop();else if(field_==LocalField::Chat)chat_.Pop();else if(field_==LocalField::Message)message_.Pop();else return false;return true;}
 void EndEdit(bool cancel){if(cancel){if(field_==LocalField::Token)token_.Clear();if(field_==LocalField::Message)message_.Clear();}field_=LocalField::None;++input_;}
 std::string_view Value(LocalField field)const{if(field==LocalField::Vm)return vm_.View();if(field==LocalField::Chat)return chat_.View();if(field==LocalField::Message)return message_.View();return {};}
 size_t TokenCount()const{return token_.Size();}
 bool Stage(uint64_t captured){
  if(captured!=epoch_||!Approved()||!port_.PairIdle()||!port_.ChatIdle())return false;
  if(mode_==LocalMode::Pair&&(token_.Size()!=43||std::string_view("AEIMQUYcgkosw048").find(token_.View().back())==std::string_view::npos))return false;
  if(mode_==LocalMode::Chat&&(!Id(vm_.View())||!Id(chat_.View())))return false;
  auto context=std::make_shared<LocalScope>(port_,mode_);context->expires=port_.Now()+(mode_==LocalMode::Pair?600000:120000);context->network=port_.Borrow(network_);if(!context->network||!context->NetworkCurrent())return false;
  context->expires=port_.Now()+(mode_==LocalMode::Pair?600000:120000);if(!port_.RandomId(context->generation))return false;
  bool accepted=false;
  if(mode_==LocalMode::Pair){
   // Fixed storage, one bounded copy. Never put the SDK token in std::string.
   std::array<char,49> value{};std::memcpy(value.data(),"mgst_",5);std::memcpy(value.data()+5,token_.View().data(),43);
   PairingGrant grant;grant.node=identity_.node;grant.device=identity_.device;grant.mac=identity_.mac;grant.version=identity_.version;grant.expires_ms=context->expires;grant.enrollment_current=[context]{return context->Current();};grant.storage_current=grant.enrollment_current;grant.network_current=[context]{return context->NetworkCurrent();};
   accepted=port_.PreparePair(grant,std::string_view(value.data(),48));for(volatile char& c:value)c=0;token_.Clear();
  }else{
   SessionGrant grant;grant.node=identity_.node;grant.vm=std::string(vm_.View());grant.chat=std::string(chat_.View());grant.generation=context->generation;grant.expires_ms=context->expires;grant.credential_current=[context]{return context->Current();};grant.chat_current=grant.credential_current;grant.network_current=[context]{return context->NetworkCurrent();};accepted=port_.PrepareChat(grant);
  }
  if(!accepted){context->active=false;return false;}scope_=std::move(context);field_=LocalField::None;++input_;return true;
 }
 bool Send(uint64_t captured){
  if(captured!=epoch_||!Time()||!scope_||scope_->mode!=LocalMode::Chat||!scope_->Current()||message_generation_!=scope_->generation||!message_.Size())return false;
  std::string request;if(!port_.RandomId(request))return false;std::string text(message_.View());const bool sent=port_.Text(message_generation_,request,text);Wipe(text);message_.Clear();message_generation_.clear();return sent;
 }
 void Poll(){if(!Time()){Close();return;}if(scope_&&(!scope_->NetworkCurrent()||(scope_->mode==LocalMode::Pair?port_.PairIdle():port_.ChatIdle())))CancelScope();if(token_.Size()&&(port_.Now()>=token_until_||!Approved()))token_.Clear();}
 bool OwnsChat()const{return scope_&&scope_->mode==LocalMode::Chat&&scope_->active;}
 bool HasScope()const{return scope_&&scope_->active;}
};
}
