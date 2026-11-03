#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <algorithm>
namespace direct_usb_pcm {
inline uint32_t Le32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline uint16_t Le16(const uint8_t* p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline void Put32(uint8_t* p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=uint8_t(n>>(8*i));}
inline uint32_t Crc(const uint8_t* b,size_t n){uint32_t c=~uint32_t{0};for(size_t i=0;i<n;++i){c^=b[i];for(unsigned bit=0;bit<8;++bit)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
struct FrameView {uint8_t kind;uint16_t flags,stream;uint32_t sequence,rate;const uint8_t* payload;size_t size;};
inline bool DecodeFrame(const uint8_t* p,size_t n,FrameView& v){
 if(!p||n<24||n>4120||Le16(p)!=0x5343||p[2]!=2||p[3]>2)return false;
 const auto bytes=Le32(p+16);if(bytes>4096||n!=24+bytes||Crc(p,n-4)!=Le32(p+n-4))return false;
 v={p[3],Le16(p+4),Le16(p+6),Le32(p+8),Le32(p+12),p+20,bytes};return true;
}
// One record at a time, no queue allocation or resynchronization on malformed
// authenticated input. Callback bytes are borrowed only during the call.
class RecordParser {
 std::array<uint8_t,4125> bytes_{};size_t used_=0,expected_=5;bool failed_=false;
public:
 template<class Sink> bool Feed(const uint8_t* p,size_t n,Sink&& sink){
  if(failed_||(!p&&n))return false;
  while(n){const auto take=std::min(n,expected_-used_);std::copy_n(p,take,bytes_.data()+used_);p+=take;n-=take;used_+=take;
   if(used_==5&&expected_==5){const auto kind=bytes_[0];const auto size=Le32(bytes_.data()+1);if(!size||(kind!=1&&kind!=2)||size>(kind==1?2048u:4120u)){failed_=true;return false;}expected_=5+size;}
   if(used_==expected_){if(!sink(bytes_[0],bytes_.data()+5,used_-5)){failed_=true;return false;}used_=0;expected_=5;}
  }return true;
 }
 bool Failed()const{return failed_;}void Reset(){used_=0;expected_=5;failed_=false;}
};
}
