#include "usb_tls_client.h"
#include "local_tls_policy.hpp"
#include "local_tls_requirements.hpp"
#include <esp_timer.h>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <exception>
#include <limits>
extern "C" const char* companion_local_ca_pem();
extern "C" const char* companion_local_tls_host();
namespace companion {
namespace {
uint64_t Now(){return uint64_t(esp_timer_get_time()/1000);}
bool ClockReady(){
#ifndef MBEDTLS_HAVE_TIME_DATE
 return false;
#else
 return std::time(nullptr)>=1735689600;
#endif
}
void Wipe(uint8_t* p,size_t n){volatile uint8_t* v=p;while(n--)*v++=0;}
bool Pending(int n){return n==MBEDTLS_ERR_SSL_WANT_READ||n==MBEDTLS_ERR_SSL_WANT_WRITE;}
}
UsbTlsClient::UsbTlsClient(){Init();}
bool UsbTlsClient::Verified()const{return state_==UsbTlsState::Ready&&owner_&&owner_->Owned()&&owner_->ChannelId()==channel_&&ClockReady();}
UsbTlsClient::~UsbTlsClient(){if(owner_||state_!=UsbTlsState::Closed)std::terminate();Free();}
void UsbTlsClient::Init(){mbedtls_ssl_init(&ssl_);mbedtls_ssl_config_init(&config_);mbedtls_x509_crt_init(&ca_);mbedtls_entropy_init(&entropy_);mbedtls_ctr_drbg_init(&random_);initialized_=true;}
void UsbTlsClient::Free(){if(!initialized_)return;mbedtls_ssl_free(&ssl_);mbedtls_ssl_config_free(&config_);mbedtls_x509_crt_free(&ca_);mbedtls_ctr_drbg_free(&random_);mbedtls_entropy_free(&entropy_);initialized_=false;}
void UsbTlsClient::ClearWrite(){Wipe(pending_.data(),pending_.size());pending_size_=pending_offset_=0;}
bool UsbTlsClient::Guard(){return owner_&&owner_->Owned()&&owner_->ChannelId()==channel_&&ClockReady();}
int UsbTlsClient::Fail(int error){error_=error;state_=UsbTlsState::Closing;ClearWrite();Free();if(owner_)owner_->DiscardPending(channel_);return -1;}
bool UsbTlsClient::Begin(UsbByteOwner& owner,const char* host){
 if(state_!=UsbTlsState::Closed||owner_)return false;
 const char* ca=companion_local_ca_pem();const char* expected=companion_local_tls_host();
 if(!ClockReady()){error_=ClockNotReady;return false;}
 if(!host||!ca||!expected||SelectTlsRoute(ca,expected,host)!=TlsRoute::Local){error_=TrustRejected;return false;}
 if(!owner.Owned()||!owner.ChannelId()){error_=OwnerUnavailable;return false;}
 const auto now=Now();if(now>std::numeric_limits<uint64_t>::max()-IoBudgetMs)return false;
 owner_=&owner;channel_=owner.ChannelId();state_=UsbTlsState::Handshaking;error_=0;handshake_until_=now+IoBudgetMs;
 if(!initialized_)Init();
 static const unsigned char label[]="companion-owned-usb-tls";
 int ret=mbedtls_ctr_drbg_seed(&random_,mbedtls_entropy_func,&entropy_,label,sizeof(label)-1);
 if(!ret)ret=mbedtls_x509_crt_parse(&ca_,reinterpret_cast<const unsigned char*>(ca),std::strlen(ca)+1);
 if(!ret)ret=mbedtls_ssl_config_defaults(&config_,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);
 if(ret){Fail(ret);return false;}
 mbedtls_ssl_conf_authmode(&config_,MBEDTLS_SSL_VERIFY_REQUIRED);
 mbedtls_ssl_conf_ca_chain(&config_,&ca_,nullptr);
 mbedtls_ssl_conf_rng(&config_,mbedtls_ctr_drbg_random,&random_);
 mbedtls_ssl_conf_min_tls_version(&config_,MBEDTLS_SSL_VERSION_TLS1_2);
 ret=mbedtls_ssl_setup(&ssl_,&config_);
 if(!ret)ret=mbedtls_ssl_set_hostname(&ssl_,expected);
 if(ret){Fail(ret);return false;}
 mbedtls_ssl_set_bio(&ssl_,this,Send,Receive,nullptr);return true;
}
int UsbTlsClient::Send(void* ctx,const unsigned char* data,size_t size){auto& c=*static_cast<UsbTlsClient*>(ctx);if(!c.Guard()||Now()>=c.call_until_)return MBEDTLS_ERR_SSL_TIMEOUT;const auto offered=std::min(size,MaxTransportChunk);const int n=c.owner_->Write(data,offered);if(n<0)return MBEDTLS_ERR_SSL_CONN_EOF;if(size_t(n)>offered)return MBEDTLS_ERR_SSL_INTERNAL_ERROR;return n?n:MBEDTLS_ERR_SSL_WANT_WRITE;}
int UsbTlsClient::Receive(void* ctx,unsigned char* data,size_t size){auto& c=*static_cast<UsbTlsClient*>(ctx);if(!c.Guard()||Now()>=c.call_until_)return MBEDTLS_ERR_SSL_TIMEOUT;const auto offered=std::min(size,MaxTransportChunk);const int n=c.owner_->Read(data,offered);if(n<0)return MBEDTLS_ERR_SSL_CONN_EOF;if(size_t(n)>offered)return MBEDTLS_ERR_SSL_INTERNAL_ERROR;return n?n:MBEDTLS_ERR_SSL_WANT_READ;}
int UsbTlsClient::PollHandshake(){
 if(state_==UsbTlsState::Ready)return Guard()?1:Fail(MBEDTLS_ERR_SSL_CONN_EOF);
 if(state_!=UsbTlsState::Handshaking)return -1;
 if(!Guard()||Now()>=handshake_until_)return Fail(MBEDTLS_ERR_SSL_TIMEOUT);
 call_until_=handshake_until_;const int n=mbedtls_ssl_handshake_step(&ssl_);
 if(!Guard()||Now()>=handshake_until_)return Fail(MBEDTLS_ERR_SSL_TIMEOUT);
 if(Pending(n))return 0;
 if(n)return Fail(n);
 if(!mbedtls_ssl_is_handshake_over(&ssl_))return 0;
 if(mbedtls_ssl_get_verify_result(&ssl_)!=0)return Fail(MBEDTLS_ERR_SSL_BAD_INPUT_DATA);
 state_=UsbTlsState::Ready;return 1;
}
bool UsbTlsClient::BeginWrite(const uint8_t* data,size_t size){if(state_!=UsbTlsState::Ready||!data||!size||size>MaxPlaintext||pending_size_)return false;
 if(!Guard()){Fail(MBEDTLS_ERR_SSL_CONN_EOF);return false;}const auto now=Now();if(now>std::numeric_limits<uint64_t>::max()-IoBudgetMs)return false;std::memcpy(pending_.data(),data,size);pending_size_=size;pending_offset_=0;write_until_=now+IoBudgetMs;return true;}
int UsbTlsClient::PollWrite(){
 if(state_!=UsbTlsState::Ready)return -1;
 if(!Guard())return Fail(MBEDTLS_ERR_SSL_CONN_EOF);
 if(!pending_size_)return 1;
 if(Now()>=write_until_)return Fail(MBEDTLS_ERR_SSL_TIMEOUT);
 call_until_=write_until_;
 const int n=mbedtls_ssl_write(&ssl_,pending_.data()+pending_offset_,pending_size_-pending_offset_);
 if(!Guard()||Now()>=write_until_)return Fail(MBEDTLS_ERR_SSL_TIMEOUT);
 if(Pending(n))return 0;
 if(n<=0||size_t(n)>pending_size_-pending_offset_)return Fail(n<0?n:MBEDTLS_ERR_SSL_INTERNAL_ERROR);
 pending_offset_+=size_t(n);if(pending_offset_!=pending_size_)return 0;ClearWrite();return 1;
}
int UsbTlsClient::Read(uint8_t* data,size_t size){
 if(state_!=UsbTlsState::Ready||!data||!size||size>MaxPlaintext)return -1;
 if(!Guard())return Fail(MBEDTLS_ERR_SSL_CONN_EOF);
 const auto now=Now();if(now>std::numeric_limits<uint64_t>::max()-IoBudgetMs)return Fail(MBEDTLS_ERR_SSL_TIMEOUT);call_until_=now+IoBudgetMs;
 const int n=mbedtls_ssl_read(&ssl_,data,size);
 if(!Guard()||Now()>=call_until_){Wipe(data,size);return Fail(MBEDTLS_ERR_SSL_TIMEOUT);}
 if(Pending(n))return 0;
 if(n<=0||size_t(n)>size){Wipe(data,size);return Fail(n<0?n:MBEDTLS_ERR_SSL_CONN_EOF);}return n;
}
bool UsbTlsClient::Stop(){
 if(!owner_){Free();ClearWrite();state_=UsbTlsState::Closed;return true;}
 state_=UsbTlsState::Closing;ClearWrite();Free();const bool discarded=owner_->DiscardPending(channel_);
 const bool closed=owner_->CloseAndConfirm(channel_);
 if(!discarded||!closed||owner_->Owned())return false;
 owner_=nullptr;channel_=0;Free();state_=UsbTlsState::Closed;return true;
}
}
