#pragma once
#include "midi1.hpp"
#include <usb/usb_host.h>
#include <atomic>
#include <mutex>
namespace stackchan_midi_host {
// Future exclusive role/power owner installs/serves the Host Library separately.
// This client NEVER installs a PHY, powers VBUS, or exposes a UI start path.
// Poll belongs to one host client task; Drain belongs to the exclusive audio
// worker. Callbacks only fill the bounded SPSC event queue, never touch Engine.
class HostClient {
 usb_host_client_handle_t client_=nullptr;usb_device_handle_t device_=nullptr;usb_transfer_t* transfer_=nullptr;
 Interface interface_{};uint8_t pending_address_=0;std::atomic<bool> stopping_{false};bool gone_=false,inflight_=false,claimed_=false,submit_=false;
 std::array<Event,64> events_{};std::atomic<unsigned> head_{0},tail_{0};std::atomic<bool> panic_{false},closed_{true};std::atomic<unsigned> rejected_{0};
 std::mutex dispatch_fence_;
 static void ClientEvent(const usb_host_client_event_msg_t*,void*);
 static void Transfer(usb_transfer_t*);
 void Push(const Event&);
 bool Open(uint8_t);
 bool Close();
public:
 HostClient()=default;HostClient(const HostClient&)=delete;HostClient& operator=(const HostClient&)=delete;
 // Registration requires a real already-installed Host Library; it is not an
 // authorization or power-readiness flag. Future role manager must call this.
 bool RegisterAfterHostInstalled();
 void RequestStop(){
  stopping_=true;panic_=true;
  // Acknowledge after any already-admitted single Engine event completes.
  // No USB/codec/render call occurs under this short dispatch fence.
  std::lock_guard<std::mutex> fence(dispatch_fence_);
 }
 bool Poll(); // bounded iterations, nonblocking event service; SDK lifecycle APIs may block.
 bool Closed()const{return closed_.load();}
 unsigned Rejected()const{return rejected_;}
 template<class Engine> unsigned Drain(Engine& engine,unsigned limit=16){
  if(limit>16)limit=16;
  if(stopping_.load()){tail_.store(head_.load(std::memory_order_acquire),std::memory_order_release);engine.AllNotesOff();return 0;}
  if(panic_.exchange(false)){tail_.store(head_.load(std::memory_order_acquire),std::memory_order_release);engine.AllNotesOff();}unsigned count=0;
  while(count<limit){unsigned tail=tail_.load(std::memory_order_relaxed);if(tail==head_.load(std::memory_order_acquire))break;const auto event=events_[tail];tail_.store((tail+1)%events_.size(),std::memory_order_release);++count;
   std::lock_guard<std::mutex> fence(dispatch_fence_);
   if(stopping_.load()){tail_.store(head_.load(std::memory_order_acquire),std::memory_order_release);engine.AllNotesOff();return count;}
   switch(event.kind){case Kind::NoteOn:if(!engine.NoteOn(event.channel,event.note,event.value))++rejected_;break;case Kind::NoteOff:engine.NoteOff(event.channel,event.note);break;case Kind::Sustain:engine.Sustain(event.channel,event.value>=64);break;case Kind::AllOff:engine.AllNotesOff();break;default:break;}
  }return count;
 }
};
}
