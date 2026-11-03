// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include <atomic>
#include <functional>
#include <memory>
#include <cassert>
namespace companion::muse {
// Lifecycle controller serializes Start/TryRetire/destruction; publish Start's
// return before another controller may retire. Only Stop/Done cross actors.
// Kernel owns a private handle which must never be resumed/exposed. A failed
// Create with no handle guarantees no entry execution; ambiguous results retain.
template<class Work,class Kernel>
class RetainedVoiceTask final {
 using Handle=typename Kernel::Handle;
 Kernel& kernel_;std::function<std::unique_ptr<Work>(std::function<bool()>)> factory_;
 Handle handle_{};bool attempted_=false,created_=false;
 std::atomic<bool> stop_{false},done_{false},retired_{false};
 static void Entry(void* context){static_cast<RetainedVoiceTask*>(context)->Run();}
 void Run(){
  std::unique_ptr<Work> work;
  if(!stop_.load())work=factory_([this]{return !stop_.load();});
  const bool opened=work&&work->Begin();
  while(opened&&!stop_.load()&&work->Poll())kernel_.Delay(5);
  stop_.store(true);
  if(work){work->SignalClose();while(!work->Close())kernel_.Delay(20);work.reset();}
  // No Work, scope callback or owned widget/SDK resource survives this point.
  done_.store(true);
  for(;;)kernel_.SuspendSelf(); // done is not a suspension/join receipt
 }
public:
 RetainedVoiceTask(Kernel&k,std::function<std::unique_ptr<Work>(std::function<bool()>)> factory):kernel_(k),factory_(std::move(factory)){}
 RetainedVoiceTask(const RetainedVoiceTask&)=delete;RetainedVoiceTask&operator=(const RetainedVoiceTask&)=delete;
 ~RetainedVoiceTask(){assert(!attempted_||retired_.load());}
 bool Start(unsigned stack_bytes){
  if(attempted_||stop_.load()||!factory_||stack_bytes<8192)return false;
  attempted_=true;const bool result=kernel_.Create(Entry,this,stack_bytes,handle_);created_=result||bool(handle_);
  if(!created_)done_.store(true); // definitive no task, no entry/IO
  return result&&bool(handle_);
 }
 void SignalStop(){stop_.store(true);}bool StopRequested()const{return stop_.load();}
 bool Done()const{return done_.load();}
 bool TryRetire(){
  if(retired_.load())return true;
  if(!attempted_||!created_){factory_={};retired_.store(true);return true;}
  if(!handle_||!done_.load()||!kernel_.Suspended(handle_))return false;
  kernel_.Delete(handle_);handle_={};factory_={};retired_.store(true);return true;
 }
 bool CleanupComplete()const{return retired_.load();}
};
}
#endif
