// SPDX-License-Identifier: Apache-2.0
#include "muse_direct_native_api.h"
#include <esp_timer.h>
#include <map>
namespace companion::muse {
namespace {uint64_t Now(){return uint64_t(esp_timer_get_time()/1000);}}
bool NativeVmLookup::Current(){if(revoked_||!current_||!current_()){revoked_=true;return false;}const auto now=Now();if(now<last_||now>=end_){revoked_=true;return false;}last_=now;return true;}
bool NativeVmLookup::Write(std::string& text){size_t done=0;while(done<text.size()&&Current()){const int n=stream_.Write(reinterpret_cast<const uint8_t*>(text.data()+done),text.size()-done,20);if(n<0||size_t(n)>text.size()-done)return false;done+=n;}return done==text.size()&&Current();}
bool NativeVmLookup::Read(uint8_t* out,size_t size){size_t done=0;while(done<size&&Current()){const int n=stream_.Read(out+done,size-done,20);if(n<0||size_t(n)>size-done)return false;done+=n;}return done==size&&Current();}
bool NativeVmLookup::Line(std::string& out,size_t limit){Wipe(out);out.reserve(limit);while(out.size()<limit&&Current()){uint8_t c=0;if(!Read(&c,1))return false;out+=char(c);if(out.size()>=2&&out.compare(out.size()-2,2,"\r\n")==0){out.resize(out.size()-2);return true;}if(c==0)return false;}return false;}
bool NativeVmLookup::FindExactVm(const DeviceRecord& record,const std::string& vm,VmCredential& output){
 output.Clear();if(spent_||!Id(vm)||!record.access.Size()||!current_||!current_())return false;
 spent_=true;last_=Now();end_=last_+10000; // Five-second DNS/TLS budget plus bounded HTTP phase.
 if(!stream_.ConnectFixedMeta(5000)||!stream_.PeerVerified()||!Current()){CloseAndJoin(0);return false;}
 std::string request;request.reserve(4096);request="GET /fetch_vms HTTP/1.1\r\nHost: api.muse.ai\r\nAuthorization: Bearer ";request+=record.access.Get();request+="\r\nX-API-Version: 1.0.0\r\nAccept: application/json\r\nConnection: close\r\n\r\n";
 const bool sent=Write(request);Wipe(request);if(!sent){CloseAndJoin(0);return false;}
 struct TextBuffer{std::string value;~TextBuffer(){Wipe(value);}};TextBuffer line_buffer,body_buffer;auto& line=line_buffer.value;std::map<std::string,std::string> headers;size_t bytes=0;bool okay=Line(line,1024)&&line.rfind("HTTP/1.1 200 ",0)==0;bytes=line.size()+2;
 bool ended=false;while(okay&&bytes<4096){if(!Line(line,1024)){okay=false;break;}bytes+=line.size()+2;if(line.empty()){ended=true;break;}const auto split=line.find(':');if(split==std::string::npos||split==0||line[0]==' '||line[0]=='\t'){okay=false;break;}auto key=companion::lower(line.substr(0,split));if(headers.size()>=32||headers.count(key)){okay=false;break;}auto value=(key=="content-length"||key=="transfer-encoding")?companion::trim(line.substr(split+1)):std::string();headers.emplace(std::move(key),std::move(value));}
 okay=okay&&ended&&bytes<=4096;
 auto& body=body_buffer.value;body.reserve(8192);
 const auto length=headers.find("content-length"),encoding=headers.find("transfer-encoding");
 if(okay&&length!=headers.end()&&encoding==headers.end()){
  size_t count=0;if(length->second.empty())okay=false;for(char c:length->second){if(c<'0'||c>'9'||count>8192){okay=false;break;}count=count*10+size_t(c-'0');}if(count>8192)okay=false;
  if(okay){body.resize(count);okay=Read(reinterpret_cast<uint8_t*>(body.data()),count);}
 }else if(okay&&length==headers.end()&&encoding!=headers.end()&&companion::lower(encoding->second)=="chunked"){
  bool final=false;for(unsigned chunks=0;okay&&chunks<128;++chunks){if(!Line(line,8)||line.empty()){okay=false;break;}size_t count=0;for(char c:line){unsigned n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:16;if(n>15||count>8192){okay=false;break;}count=count*16+n;}if(!okay||count>8192-body.size()){okay=false;break;}if(!count){final=Line(line,2)&&line.empty();break;}const size_t old=body.size();body.resize(old+count);okay=Read(reinterpret_cast<uint8_t*>(body.data()+old),count)&&Line(line,2)&&line.empty();}okay=okay&&final;
 }else okay=false;
 const bool joined=CloseAndJoin(0);if(!okay||!joined||!Current()){Wipe(body);return false;}
 Json root(Parse(body));Wipe(body);okay=SelectExactVm(root.value,vm,output)&&Current();if(!okay)output.Clear();return okay;
}
}
