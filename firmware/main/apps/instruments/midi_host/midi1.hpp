#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
namespace stackchan_midi_host {
enum class Kind : uint8_t { NoteOn,NoteOff,Sustain,AllOff,PitchBend,Ignored,Invalid };
struct Event { Kind kind=Kind::Invalid; uint8_t channel=0,note=0,value=0;uint16_t bend=8192; };
inline Event Packet(const uint8_t* p,size_t n,uint8_t cables=1) {
 if(!p||n!=4||!cables||(p[0]>>4)>=cables)return {};
 const unsigned cin=p[0]&15,status=p[1];Event e{};e.channel=status&15;
 if(cin==8||cin==9||cin==11||cin==14){
  if((status>>4)!=cin||p[2]>127||p[3]>127)return {};
  e.note=p[2];e.value=p[3];
  if(cin==8||(cin==9&&!p[3]))e.kind=Kind::NoteOff;
  else if(cin==9)e.kind=Kind::NoteOn;
  else if(cin==11){if(p[2]==64)e.kind=Kind::Sustain;else if((p[2]==120||p[2]==123)&&p[3]==0)e.kind=Kind::AllOff;else e.kind=Kind::Ignored;}
  else {e.kind=Kind::PitchBend;e.bend=uint16_t(p[2]|(p[3]<<7));}
  return e;
 }
 // Unsupported valid fixed-channel messages never reach the instrument engine.
 if((cin==10||cin==12||cin==13)&&(status>>4)==cin&&p[2]<128&&((cin==10&&p[3]<128)||((cin==12||cin==13)&&p[3]==0))){e.kind=Kind::Ignored;return e;}
 if(cin==15&&status>=0xf8&&p[2]==0&&p[3]==0){e.kind=Kind::Ignored;return e;}
 // SysEx/system-common unsupported rather than partly interpreted.
 return {};
}
struct Interface {uint8_t number=0,endpoint=0,cables=0;uint16_t packet_size=0;};
inline uint16_t U16(const uint8_t* p){return uint16_t(p[0]|(p[1]<<8));}
// Strict first prototype: one MIDI1 alt0 interface, class-compliant AC link,
// <=1024 config bytes, <=16 interfaces, full-speed bulk IN <=64 bytes.
inline bool Descriptor(const uint8_t* p,size_t n,Interface& out){
 if(!p||n<9||n>1024||p[0]!=9||p[1]!=2||U16(p+2)!=n||!p[4]||p[4]>16||!p[5])return false;
 struct Jack {uint8_t type=0,dir=0;};std::array<Jack,256> jacks{};
 std::array<bool,256> ac_links{},sources{},interfaces{};unsigned interface_count=0;bool audio=false,midi=false,header=false,endpoint_cs=false;unsigned found=0,declared=0,actual=0;size_t ms_end=0;Interface candidate{};bool previous_in=false,previous_endpoint=false;
 for(size_t offset=9;offset<n;){
  if(n-offset<2)return false;
  const uint8_t len=p[offset],type=p[offset+1];if(len<2||offset+len>n)return false;
  const auto d=p+offset;if((type==0x24||type==0x25)&&len<3)return false;
  if(type==4){
   if(len!=9||d[2]>=p[4])return false;
   if(d[3]==0){if(interfaces[d[2]])return false;interfaces[d[2]]=true;++interface_count;}
   if(midi && (!header||!endpoint_cs||actual!=declared||ms_end!=offset))return false;
   audio=d[5]==1&&d[6]==1&&d[7]==0&&d[3]==0;
   midi=d[5]==1&&d[6]==3&&d[7]==0&&d[3]==0;previous_in=false;previous_endpoint=false;
   if(midi){if(++found!=1||d[4]<1||d[4]>2)return false;candidate.number=d[2];declared=d[4];actual=0;header=false;endpoint_cs=false;jacks.fill({});}
  }else if(audio&&type==0x24&&d[2]==1){
   if(len<9||U16(d+3)!=0x0100||len!=8+d[7]||!d[7]||U16(d+5)<len)return false;
   for(unsigned i=8;i<len;++i)ac_links[d[i]]=true;
  }else if(midi&&type==0x24){
   if(len<3)return false;
   previous_in=false;
   if(d[2]==1){if(header||len!=7||U16(d+3)!=0x0100||U16(d+5)<7)return false;header=true;ms_end=offset+U16(d+5);if(ms_end>n)return false;}
   else if(d[2]==2){if(!header||offset+len>ms_end||len!=6||d[3]<1||d[3]>2||!d[4]||jacks[d[4]].type)return false;jacks[d[4]]={d[3],2};}
   else if(d[2]==3){if(!header||offset+len>ms_end||len<9||!d[5]||len!=7+2*d[5]||d[3]<1||d[3]>2||!d[4]||jacks[d[4]].type)return false;for(unsigned i=0;i<d[5];++i){if(!d[6+2*i]||d[7+2*i]!=1)return false;sources[d[6+2*i]]=true;}jacks[d[4]]={d[3],3};}
   else return false; // Element/UMP extensions need a separate qualified parser.
  }else if(midi&&type==5){
   if(!header||offset+len>ms_end||len!=9||d[3]!=2||!(d[2]&15)||(d[2]&0x70)||(U16(d+4)!=8&&U16(d+4)!=16&&U16(d+4)!=32&&U16(d+4)!=64))return false;
   ++actual;previous_endpoint=true;previous_in=(d[2]&0x80)!=0;
   if(previous_in){if(candidate.endpoint)return false;candidate.endpoint=d[2];candidate.packet_size=U16(d+4);}
  }else if(midi&&type==0x25){
   if(!previous_endpoint||offset+len>ms_end||len<5||d[2]!=1||d[3]<1||d[3]>16||len!=4+d[3])return false;
   std::array<bool,256> ids{};for(unsigned i=4;i<len;++i){if(!d[i]||ids[d[i]]||jacks[d[i]].type!=1||jacks[d[i]].dir!=(previous_in?3:2))return false;ids[d[i]]=true;}
   if(previous_in){if(endpoint_cs)return false;candidate.cables=d[3];endpoint_cs=true;}previous_in=false;previous_endpoint=false;
  }else {previous_in=false;previous_endpoint=false;}
  offset+=len;
 }
 if((midi&&ms_end!=n)||interface_count!=p[4]||found!=1||!header||!endpoint_cs||!candidate.endpoint||!ac_links[candidate.number]||actual!=declared)return false;
 for(unsigned i=1;i<sources.size();++i)if(sources[i]&&jacks[i].dir!=2)return false;
 out=candidate;return true;
}
}
