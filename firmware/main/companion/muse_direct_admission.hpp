// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_credentials.hpp"
#include "muse_direct_noise.hpp"
#include <memory>
#include <new>
namespace companion::muse {
struct SessionGrant {
 std::string node,vm,chat,generation;uint64_t expires_ms=0;
 // Independently authorized scope checks; a Muse selection or pairing is not
 // a credential-read, network, VM/chat or text-submission grant.
 // Checks must be thread-safe, bounded and must not reenter these APIs.
 std::function<bool()> credential_current,network_current,chat_current;
};
class AdmittedSession {
 DeviceCredentialReader& reader_;ExactVmLookup& lookup_;NoiseChatOwner& chat_;wire::CryptoBackend& crypto_;
 SessionGrant grant_;std::function<uint64_t()> clock_;std::function<bool()> foreground_;std::function<void(const std::string&,bool)> caption_;
 bool active_=false,spent_=false,submitted_=false;uint64_t last_=0;
 bool Current(){if(!active_||!clock_||!foreground_||!foreground_())return false;const auto now=clock_();if(now<last_||now>=grant_.expires_ms)return false;last_=now;return grant_.credential_current&&grant_.network_current&&grant_.chat_current&&grant_.credential_current()&&grant_.network_current()&&grant_.chat_current();}
public:
 AdmittedSession(DeviceCredentialReader& reader,ExactVmLookup& lookup,NoiseChatOwner& chat,wire::CryptoBackend& crypto):reader_(reader),lookup_(lookup),chat_(chat),crypto_(crypto){}
 bool Open(const SessionGrant& grant,std::function<uint64_t()> clock,std::function<bool()> foreground,std::function<void(const std::string&,bool)> caption){
  if(spent_||!clock||!foreground||!foreground()||!Id(grant.node)||!Id(grant.vm)||!Id(grant.chat)||!Id(grant.generation,64)||!grant.credential_current||!grant.network_current||!grant.chat_current||!grant.credential_current()||!grant.network_current()||!grant.chat_current())return false;
  const auto now=clock();if(grant.expires_ms<=now||grant.expires_ms-now>120000)return false;
  spent_=true;active_=true;grant_=grant;clock_=std::move(clock);foreground_=std::move(foreground);caption_=std::move(caption);last_=now;
  std::unique_ptr<DeviceRecord> record(new(std::nothrow)DeviceRecord());std::unique_ptr<VmCredential> vm(new(std::nothrow)VmCredential());
  if(!record||!vm||!Current()||!reader_.LoadAccess(grant_.node,[this]{return Current();},*record)||record->node!=grant_.node||!record->access.Size()||!Current()){Close();return false;}
  record->refresh.Clear();
  const bool found=lookup_.FindExactVm(*record,grant_.vm,*vm);record->Clear();
  if(!found||vm->vm!=grant_.vm||!vm->bearer.Size()||!Current()){vm->Clear();Close();return false;}
  const bool opened=chat_.Open(crypto_,grant_.vm,vm->bearer.Get(),grant_.node,grant_.chat,grant_.generation,grant_.expires_ms,clock_,[this]{return Current();},[this](const std::string& text,bool end){if(caption_&&(text.empty()||Current()))caption_(text,end);});
  vm->Clear();if(!opened||!Current()){Close();return false;}return true;
 }
 bool Poll(){if(!Current()){Close();return false;}return chat_.Poll();}
 bool Ready(){return Current()&&chat_.Ready();}
 bool Submit(const std::string& captured_generation,const std::string& request,const std::string& text){
  if(!Ready()||submitted_||captured_generation!=grant_.generation||!Id(request,64)||text.empty()||text.size()>256)return false;
  for(unsigned char c:text)if(c<32||c>126)return false;
  submitted_=true;return chat_.Submit(request,text);
 }
 bool Close(){active_=false;const bool joined=chat_.Close();if(caption_)caption_("",false);caption_={};foreground_={};clock_={};grant_={};return joined;}
};
}
