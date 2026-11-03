#pragma once
namespace usb_composite_v2 {
// UI caller owns only a successful Start receipt. PollStopped is the existing
// LocalAudio joined cleanup fence; a busy foreign lease is never stopped.
class ControlsSession {
 bool owned_=false,armed_=false,stopping_=false;
public:
 bool Owned()const{return owned_;}bool Armed()const{return armed_;}
 template<class IO> bool Start(IO& io){
  if(owned_||!io.Ready()||!io.PollStopped()||!io.Start())return false;
  owned_=true;armed_=stopping_=false;return true;
 }
 template<class IO> void Stop(IO& io){
  if(!owned_)return;
  io.Disarm();armed_=false;stopping_=true;io.RequestStop();
 }
 template<class IO> void Tick(IO& io){
  if(!owned_)return;
  if(stopping_){if(io.PollStopped()){owned_=armed_=stopping_=false;}return;}
  if(!io.Connected()||io.Fault()){Stop(io);return;}
  // Audio startup is asynchronous. Incoming MIDI stays disarmed until running.
  if(!armed_&&io.Running()){
   if(io.Arm())armed_=true;else Stop(io);
  }
 }
};
}
