// SPDX-License-Identifier: Apache-2.0
#include "muse_direct_native_socket.h"
#include <esp_tls.h>
#include <esp_crt_bundle.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ctime>
#include <climits>
namespace companion::muse {
namespace {
uint64_t Millis(){return uint64_t(esp_timer_get_time()/1000);}
bool Budget(uint64_t begin,unsigned timeout){const auto now=Millis();return now>=begin&&now-begin<timeout;}
}
bool NativeMetaStream::Current(){if(revoked_||!owner_||xTaskGetCurrentTaskHandle()!=owner_||!current_||!current_()){revoked_=true;return false;}return true;}
bool NativeMetaStream::PeerVerified()const{return verified_&&!revoked_&&owner_&&xTaskGetCurrentTaskHandle()==owner_&&current_&&current_();}
void NativeMetaStream::Resolve(void* context){
 auto* owner=static_cast<NativeMetaStream*>(context);addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;addrinfo* result=nullptr;
 const int status=getaddrinfo(owner->Host(),nullptr,&hints,&result);
 if(!status&&!owner->revoked_){for(auto* p=result;p;p=p->ai_next){if(p->ai_family!=AF_INET||p->ai_addrlen<sizeof(sockaddr_in)||!p->ai_addr)continue;auto* address=reinterpret_cast<sockaddr_in*>(p->ai_addr);if(inet_ntop(AF_INET,&address->sin_addr,owner->numeric_,sizeof(owner->numeric_))){owner->resolved_=true;break;}}}
 if(result)freeaddrinfo(result);
 // Publish only after resolver-owned allocations have been released. The
 // retaining owner deletes this parked task externally; no SDK global handle.
 owner->resolver_done_=true;for(;;)vTaskSuspend(nullptr);
}
bool NativeMetaStream::JoinResolver(unsigned timeout){
 if(!resolver_)return true;
 if(xTaskGetCurrentTaskHandle()!=owner_)return false;
 const auto begin=Millis();while(!resolver_done_&&Budget(begin,timeout))vTaskDelay(pdMS_TO_TICKS(1));
 if(!resolver_done_)return false;
 vTaskDelete(resolver_);resolver_=nullptr;return true;
}
bool NativeMetaStream::ConnectFixedMeta(unsigned timeout){
 if(spent_||revoked_||!current_||!current_()||!timeout||timeout>5000||std::time(nullptr)<1735689600)return false;
 spent_=true;clean_=false;owner_=xTaskGetCurrentTaskHandle();const auto begin=Millis();resolver_done_=false;
 if(xTaskCreate(Resolve,"muse_dns",4096,this,3,&resolver_)!=pdPASS){resolver_done_=true;CloseAndJoin(0);return false;}
 while(!resolver_done_&&Current()&&Budget(begin,timeout))vTaskDelay(pdMS_TO_TICKS(1));
 if(!Current()||!resolver_done_||!JoinResolver(0)||!resolved_||!Budget(begin,timeout)){CloseAndJoin(0);return false;}
 tls_=esp_tls_init();if(!tls_){CloseAndJoin(0);return false;}
 esp_tls_cfg_t config{};config.non_block=true;config.timeout_ms=1;config.common_name=Host();config.skip_common_name=false;config.crt_bundle_attach=esp_crt_bundle_attach;config.tls_version=ESP_TLS_VER_TLS_1_2;
 // The address is a numeric IPv4 literal produced by our retained resolver.
 // IDF/lwIP recognizes numeric literals without another DNS lookup. The fixed
 // common_name supplies both SNI and certificate hostname verification.
 while(Current()&&Budget(begin,timeout)){
  const int result=esp_tls_conn_new_async(numeric_,std::strlen(numeric_),443,&config,tls_);
  if(result<0)break;
  if(result==1){int fd=-1;if(esp_tls_get_conn_sockfd(tls_,&fd)!=ESP_OK||fd<0)break;const int flags=fcntl(fd,F_GETFL,0);if(flags<0||!(flags&O_NONBLOCK))break;verified_=true;return true;}
  vTaskDelay(pdMS_TO_TICKS(1));
 }
 CloseAndJoin(0);return false;
}
int NativeMetaStream::Io(uint8_t* output,const uint8_t* input,size_t size,unsigned timeout){
 if(!PeerVerified()||!size||size>INT_MAX||!timeout||timeout>2000)return -1;
 const auto begin=Millis();while(Current()&&Budget(begin,timeout)){
  const int count=output?esp_tls_conn_read(tls_,output,size):esp_tls_conn_write(tls_,input,size);
  if(count==ESP_TLS_ERR_SSL_WANT_READ||count==ESP_TLS_ERR_SSL_WANT_WRITE){vTaskDelay(pdMS_TO_TICKS(1));continue;}
  if(count<=0||size_t(count)>size){revoked_=true;verified_=false;return -1;}return count;
 }
 return Current()?0:-1;
}
int NativeMetaStream::Read(uint8_t* data,size_t size,unsigned timeout){return data?Io(data,nullptr,size,timeout):-1;}
int NativeMetaStream::Write(const uint8_t* data,size_t size,unsigned timeout){return data&&timeout<=1000?Io(nullptr,data,size,timeout):-1;}
bool NativeMetaStream::CloseAndJoin(unsigned timeout){
 revoked_=true;verified_=false;
 if(!owner_){clean_=true;return true;}
 if(xTaskGetCurrentTaskHandle()!=owner_)return false;
 if(!JoinResolver(std::min(timeout,2000u)))return false;
 if(tls_){const int result=esp_tls_conn_destroy(tls_);tls_=nullptr;if(result!=0)close_uncertain_=true;}
 for(volatile char& c:numeric_)c=0;
 if(close_uncertain_)return false; // SDK frees TLS even if close(fd) reports failure; never retry an uncertain/reused fd.
 current_={};clean_=true;return true;
}
bool NativeWebSocketEntropy::Mask(uint8_t out[4]){if(!out)return false;esp_fill_random(out,4);return true;}
bool NativeWebSocketEntropy::Nonce(char key[25],char expected[29]){
 if(!key||!expected)return false;
 uint8_t random[16]{},digest[20]{};esp_fill_random(random,sizeof(random));size_t written=0;
 const bool encoded=mbedtls_base64_encode(reinterpret_cast<unsigned char*>(key),25,&written,random,sizeof(random))==0&&written==24;
 for(volatile uint8_t& c:random)c=0;
 if(!encoded)return false;
 const std::string input=std::string(key,24)+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
 const bool hashed=mbedtls_sha1(reinterpret_cast<const unsigned char*>(input.data()),input.size(),digest)==0;
 const bool accepted=hashed&&mbedtls_base64_encode(reinterpret_cast<unsigned char*>(expected),29,&written,digest,sizeof(digest))==0&&written==28;
 for(volatile uint8_t& c:digest)c=0;
 return accepted;
}
DirectMetaSocket::DirectMetaSocket(std::function<bool()> current):stream_(current),socket_(stream_,entropy_,Millis,std::move(current)){}
}
