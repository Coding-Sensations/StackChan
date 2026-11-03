#include "usb_control_dispatcher.h"
namespace direct_usb_pcm {
namespace {constexpr uint32_t capabilities=(1u<<0)|(1u<<1)|(1u<<2)|(1u<<4)|(1u<<5)|(1u<<9);}
bool ControlDispatcher::Fail(){failed_=true;worker_.RequestStop();return false;}
bool ControlDispatcher::Reply(uint16_t code,uint16_t stream,uint32_t rate,const uint8_t* payload,size_t n){
 if(!reply_||outgoing_==UINT32_MAX||n>8)return Fail();
 std::array<uint8_t,32> b{};b[0]=0x43;b[1]=0x53;b[2]=2;b[4]=uint8_t(code);b[5]=uint8_t(code>>8);b[6]=uint8_t(stream);b[7]=uint8_t(stream>>8);Put32(b.data()+8,outgoing_++);Put32(b.data()+12,rate);Put32(b.data()+16,n);if(n)std::copy_n(payload,n,b.data()+20);Put32(b.data()+20+n,Crc(b.data(),20+n));return reply_(b.data(),24+n)||Fail();
}
bool ControlDispatcher::Handle(const uint8_t* bytes,size_t n){
 FrameView f{};if(failed_||!DecodeFrame(bytes,n,f))return Fail();
 if(f.kind==2){if(!compatible_||!speaker_||ending_||f.flags||f.stream!=identity_.stream||f.rate!=24000||f.size!=3840||outstanding_<f.size)return Fail();if(!worker_.Receive(f.stream,f.sequence,f.rate,f.payload,f.size))return Fail();outstanding_-=f.size;return true;}
 if(f.kind!=0||f.sequence!=incoming_||incoming_==UINT32_MAX)return Fail();
 ++incoming_;
 if(f.flags==1){if(compatible_||f.stream||f.size!=8||Le32(f.payload)!=4096||(Le32(f.payload+4)&capabilities)!=capabilities)return Fail();compatible_=true;uint8_t data[8];Put32(data,4096);Put32(data+4,capabilities);return Reply(2,0,0,data,8);}
 if(!compatible_||!worker_.CurrentSession(identity_.epoch,identity_.stream)||f.stream!=identity_.stream||f.size)return Fail();
 if(f.flags==16){if(f.rate!=16000||mic_||speaker_||ending_||!worker_.BeginMic(identity_.epoch,identity_.stream))return Fail();mic_=true;return Reply(17,f.stream,16000);}
 if(f.flags==32){if(f.rate!=24000||speaker_||ending_||outstanding_||!worker_.BeginSpeaker(identity_.epoch,identity_.stream))return Fail();speaker_=true;outstanding_=8192;uint8_t c[4];Put32(c,8192);return Reply(33,f.stream,24000,c,4);}
 if(f.flags==34){if(f.rate!=24000||!speaker_||ending_)return Fail();ending_=true;return true;}
 if(f.flags==18){if(f.rate!=16000||!mic_||!worker_.StopMic(identity_.epoch,identity_.stream))return Fail();mic_=false;return Reply(19,f.stream,16000);}
 // Abort is terminal. It never emits DONE for truncated PCM.
 if(f.flags==36&&f.rate==24000){worker_.RequestStop();ending_=false;return true;}
 return Fail();
}
bool ControlDispatcher::Credit(size_t n){if(failed_||!speaker_||!n||n>8192||outstanding_>8192-n||!worker_.CurrentSession(identity_.epoch,identity_.stream))return Fail();outstanding_+=n;uint8_t c[4];Put32(c,n);return Reply(33,identity_.stream,24000,c,4);}
bool ControlDispatcher::Mic(const Identity& i,uint32_t seq,const uint8_t* pcm,size_t n){
 if(!MicCapacity()||i.epoch!=identity_.epoch||i.stream!=identity_.stream||!worker_.CurrentSession(i.epoch,i.stream)||!pcm||n!=640||!reply_)return Fail();
 std::array<uint8_t,664> b{};b[0]=0x43;b[1]=0x53;b[2]=2;b[3]=1;b[6]=uint8_t(i.stream);b[7]=uint8_t(i.stream>>8);Put32(b.data()+8,seq);Put32(b.data()+12,16000);Put32(b.data()+16,640);std::copy_n(pcm,n,b.data()+20);Put32(b.data()+660,Crc(b.data(),660));return reply_(b.data(),b.size())||Fail();
}
bool ControlDispatcher::Tick(){
 if(failed_)return false;
 if(ending_&&worker_.Status().queued==0){if(worker_.FinishSpeaker(identity_.epoch,identity_.stream)){if(!Reply(35,identity_.stream,24000))return false;ending_=speaker_=false;outstanding_=0;}}
 return true;
}
}
