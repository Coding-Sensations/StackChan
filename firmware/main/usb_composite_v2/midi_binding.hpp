#pragma once
#include <atomic>
#include <cstdint>
namespace usb_composite_v2 {
// Single USB callback producer; UI arm/disarm may occur concurrently.
// Sink::Note must only publish a coherent nonblocking audio command.
class MidiBinding {
 std::atomic<bool> armed_{false};std::atomic<unsigned> active_{0xffffffffu},rejected_{0};
public:
 void Arm(){active_=0xffffffffu;armed_=true;}
 bool Armed()const{return armed_;}unsigned Rejected()const{return rejected_;}
 template<class Sink> bool Receive(Sink& sink,unsigned channel,unsigned note,unsigned velocity){
  if(!armed_)return false;
  if(channel>15||note>126||velocity>127){++rejected_;return false;}
  const unsigned key=(channel<<8)|note;
  if(!velocity&&active_.load()!=key)return true; // Old note-off cannot mute the current monophonic note.
  active_=velocity?key:0xffffffffu;
  return sink.Note(note,velocity);
 }
 template<class Sink> void Silence(Sink& sink){if(!armed_)return;const unsigned key=active_.exchange(0xffffffffu);sink.Note(key==0xffffffffu?69:(key&255),0);}
 template<class Sink> void Disarm(Sink& sink){Silence(sink);armed_=false;}
};
}
