// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_noise.hpp"
#include "websocket_handshake.hpp"
#include <algorithm>
#include <limits>
namespace companion::muse {
inline constexpr char MetaHost[]="hatch.metaaivm.com";
// Serialized owner-thread byte port. Read/write consume no caller storage after
// returning. A pending resolver must retain its context until checked join.
class MetaByteStream {
public:
 virtual ~MetaByteStream()=default;
 virtual bool ConnectFixedMeta(unsigned timeout_ms)=0;
 virtual bool PeerVerified()const=0;
 virtual int Read(uint8_t*,size_t,unsigned timeout_ms)=0;
 virtual int Write(const uint8_t*,size_t,unsigned timeout_ms)=0;
 virtual bool CloseAndJoin(unsigned timeout_ms)=0;
};
class WebSocketEntropy {
public:
 virtual ~WebSocketEntropy()=default;
 virtual bool Nonce(char key[25],char expected_accept[29])=0;
 virtual bool Mask(uint8_t out[4])=0;
};
class MetaWebSocket final:public NoiseSocket {
 MetaByteStream& stream_;WebSocketEntropy& entropy_;
 std::function<uint64_t()> clock_;std::function<bool()> current_;
 std::array<uint8_t,wire::ClientSession::kMaxOutboundWebSocketPayloadSize> message_{};
 std::array<uint8_t,125> control_{};
 std::array<uint8_t,10> header_{};
 size_t header_used_=0,header_need_=2,payload_size_=0,payload_read_=0,message_used_=0;
 uint8_t opcode_=0;bool final_=false,frame_=false,fragment_=false,verified_=false,spent_=false,revoked_=false,clean_=true,dialed_=false;
 uint64_t last_=0;
 bool Current(){if(revoked_||!clock_||!current_||!current_())return false;const auto now=clock_();if(now<last_){revoked_=true;return false;}last_=now;return true;}
 bool Until(uint64_t end){return Current()&&last_<end;}
 uint64_t End(unsigned timeout,unsigned maximum){if(!Current()||!timeout||timeout>maximum||last_>std::numeric_limits<uint64_t>::max()-timeout)return 0;return last_+timeout;}
 unsigned Left(uint64_t end,unsigned cap=20){return Until(end)?unsigned(std::min<uint64_t>(cap,end-last_)):0;}
 bool WriteAll(const uint8_t* data,size_t size,uint64_t end){size_t offset=0;while(offset<size){const auto wait=Left(end);if(!wait)return false;const int count=stream_.Write(data+offset,size-offset,wait);if(count<0||size_t(count)>size-offset)return false;if(count)offset+=count;}return Current();}
 bool Frame(uint8_t opcode,const uint8_t* data,size_t size,uint64_t end){
  if(size>message_.size()||(size&&!data))return false;
  uint8_t head[14]{};head[0]=0x80|opcode;size_t length=2;
  if(size<126)head[1]=0x80|uint8_t(size);else if(size<=65535){head[1]=0xfe;head[2]=size>>8;head[3]=size;length=4;}else{head[1]=0xff;for(unsigned i=0;i<8;++i)head[2+i]=uint64_t(size)>>(56-8*i);length=10;}
  if(!entropy_.Mask(head+length)||!WriteAll(head,length+4,end))return false;
  uint8_t scratch[256]{};size_t offset=0;bool okay=true;
  while(offset<size){const size_t count=std::min(sizeof(scratch),size-offset);for(size_t i=0;i<count;++i)scratch[i]=data[offset+i]^head[length+(offset+i)%4];if(!WriteAll(scratch,count,end)){okay=false;break;}offset+=count;}
  for(volatile uint8_t& c:scratch)c=0;
  return okay&&Current();
 }
 int Failure(){revoked_=true;verified_=false;return -1;}
 bool Decode(){
  const uint8_t first=header_[0],second=header_[1];opcode_=first&15;final_=first&0x80;
  if((first&0x70)||(second&0x80))return false;
  uint64_t size=second&127;if(size==126){size=(uint16_t(header_[2])<<8)|header_[3];if(size<126)return false;}else if(size==127){size=0;for(unsigned i=2;i<10;++i)size=(size<<8)|header_[i];if(size<=65535||(header_[2]&0x80))return false;}
  if(opcode_>=8){if((opcode_!=8&&opcode_!=9&&opcode_!=10)||!final_||size>125||(opcode_==8&&size==1))return false;}
  else {if(opcode_!=0&&opcode_!=2)return false;if((opcode_==0)!=fragment_)return false;if(opcode_==2)message_used_=0;if(size>message_.size()-message_used_)return false;}
  payload_size_=size;payload_read_=0;frame_=true;return true;
 }
 void ResetFrame(){header_used_=0;header_need_=2;payload_read_=payload_size_=0;frame_=false;}
public:
 MetaWebSocket(MetaByteStream& stream,WebSocketEntropy& entropy,std::function<uint64_t()> clock,std::function<bool()> current):stream_(stream),entropy_(entropy),clock_(std::move(clock)),current_(std::move(current)){}
 bool ConnectMeta(const std::string& vm,const char* token,unsigned timeout)override{
  if(spent_||!clean_||!Id(vm)||!token||!token[0]||std::strlen(token)>3072)return false;
  for(const unsigned char* p=reinterpret_cast<const unsigned char*>(token);*p;++p)if(*p<33||*p>126)return false;
  const auto end=End(timeout,5000);if(!end)return false;
  spent_=true;clean_=false;dialed_=true;
  if(!stream_.ConnectFixedMeta(Left(end,5000))||!stream_.PeerVerified()||!Until(end)){CloseAndJoin(0);return false;}
  char key[25]{},accept[29]{};if(!entropy_.Nonce(key,accept)||std::strlen(key)!=24||std::strlen(accept)!=28){CloseAndJoin(0);return false;}
  std::string escaped;const char hex[]="0123456789ABCDEF";
  for(unsigned char c:vm){if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.')escaped+=char(c);else{escaped+='%';escaped+=hex[c>>4];escaped+=hex[c&15];}}
  std::string request="GET /v1/noise?vm_id="+escaped+" HTTP/1.1\r\nHost: "+MetaHost+"\r\nAuthorization: Bearer ";request.reserve(4096);request+=token;request+="\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: ";request+=key;request+="\r\n\r\n";
  const bool sent=WriteAll(reinterpret_cast<const uint8_t*>(request.data()),request.size(),end);Wipe(request);if(!sent){CloseAndJoin(0);return false;}
  std::string response;response.reserve(4096);bool complete=false;
  while(response.size()<4096){const auto wait=Left(end);if(!wait)break;uint8_t c=0;const int count=stream_.Read(&c,1,wait);if(count<0||count>1)break;if(!count)continue;response+=char(c);if(response.size()>=4&&response.compare(response.size()-4,4,"\r\n\r\n")==0){complete=true;break;}}
  verified_=complete&&companion::valid_upgrade(response,accept)&&Current()&&stream_.PeerVerified();Wipe(response);if(!verified_)CloseAndJoin(0);return verified_;
 }
 bool VerifiedMetaEndpoint()const override{return verified_&&!revoked_&&current_&&current_()&&stream_.PeerVerified();}
 bool Send(const uint8_t* data,size_t size,unsigned timeout)override{const auto end=End(timeout,1000);if(!verified_||!end||!Frame(2,data,size,end)){Failure();return false;}return true;}
 int Receive(uint8_t* output,size_t capacity,unsigned timeout)override{
  const auto end=End(timeout,2000);if(!verified_||!end||!output||!capacity)return Failure();
  for(unsigned frames=0;frames<64&&Until(end);){
   if(!frame_){
    const auto wait=Left(end);if(!wait)break;const int count=stream_.Read(header_.data()+header_used_,header_need_-header_used_,wait);if(count<0||size_t(count)>header_need_-header_used_)return Failure();if(!count)continue;header_used_+=count;
    if(header_used_<header_need_)continue;
    if(header_need_==2){header_need_=(header_[1]&127)==126?4:(header_[1]&127)==127?10:2;if(header_used_<header_need_)continue;}
    if(!Decode())return Failure();
   }
   if(payload_read_<payload_size_){const auto wait=Left(end);if(!wait)break;auto* target=opcode_>=8?control_.data()+payload_read_:message_.data()+message_used_+payload_read_;const int count=stream_.Read(target,payload_size_-payload_read_,wait);if(count<0||size_t(count)>payload_size_-payload_read_)return Failure();if(!count)continue;payload_read_+=count;if(payload_read_<payload_size_)continue;}
   ++frames;
   if(opcode_==8)return Failure();
   if(opcode_==9&&!Frame(10,control_.data(),payload_size_,end))return Failure();
   if(opcode_<8){message_used_+=payload_size_;fragment_=!final_;if(final_){const auto size=message_used_;if(size>capacity)return Failure();std::memcpy(output,message_.data(),size);for(size_t i=0;i<size;++i){volatile uint8_t& c=message_[i];c=0;}message_used_=0;ResetFrame();return int(size);}}
   ResetFrame();
  }
  if(!Current())return Failure();
  return 0;
 }
 bool CloseAndJoin(unsigned timeout)override{
  revoked_=true;verified_=false;for(volatile uint8_t& c:message_)c=0;for(volatile uint8_t& c:control_)c=0;header_.fill(0);message_used_=0;fragment_=false;ResetFrame();
  clean_=!dialed_||stream_.CloseAndJoin(std::min(timeout,2000u));if(clean_)dialed_=false;return clean_;
 }
};
}
