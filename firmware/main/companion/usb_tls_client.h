#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
namespace companion {
// One serialized caller. The provider already owns the selected USB channel.
// The provider and client must remain alive until Stop returns true.
// Discard/Close are idempotent for the captured channel, including after its close;
// a mismatched channel must never clear or close a replacement owner.
// No callback may block. Zero means would-block; negative includes terminal EOF.
class UsbByteOwner {
public:
 virtual ~UsbByteOwner()=default;
 virtual bool Owned()const=0;
 virtual uint64_t ChannelId()const=0;
 virtual int Read(uint8_t*,size_t)=0;
 virtual int Write(const uint8_t*,size_t)=0;
 virtual bool DiscardPending(uint64_t expected_channel)=0;
 virtual bool CloseAndConfirm(uint64_t expected_channel)=0;
};
enum class UsbTlsState {Closed,Handshaking,Ready,Closing};
class UsbTlsClient final {
public:
 static constexpr size_t MaxTransportChunk=4096; // Actual Serial/JTAG owner buffer limit
 static constexpr size_t MaxPlaintext=4125; //5-byte envelope +4120-byte PCM frame
 static constexpr int ClockNotReady=-0x10001, TrustRejected=-0x10002, OwnerUnavailable=-0x10003;
 static constexpr uint64_t IoBudgetMs=5000; // Existing V9 local TLS I/O budget
 UsbTlsClient();~UsbTlsClient();
 UsbTlsClient(const UsbTlsClient&)=delete;UsbTlsClient& operator=(const UsbTlsClient&)=delete;
 // Uses only existing companion_local_ca_pem()/companion_local_tls_host().
 bool Begin(UsbByteOwner&,const char* requested_host);
 int PollHandshake(); //1 verified,0 pending,-1 failed; no retries/reopen
 bool BeginWrite(const uint8_t*,size_t);
 int PollWrite(); //1 entire copied write done,0 pending,-1 failed
 int Read(uint8_t*,size_t); //actual positive plaintext,0 pending,-1 failed
 bool Stop(); //false retains exact owner custody; TLS buffers discarded; explicit retry only
 bool Verified()const;
 UsbTlsState State()const{return state_;}int LastError()const{return error_;}
 size_t PendingWrite()const{return pending_size_-pending_offset_;}
private:
 UsbByteOwner* owner_=nullptr;uint64_t channel_=0,handshake_until_=0,write_until_=0,call_until_=0;
 UsbTlsState state_=UsbTlsState::Closed;int error_=0;
 std::array<uint8_t,MaxPlaintext> pending_{};size_t pending_size_=0,pending_offset_=0;
 mbedtls_ssl_context ssl_;mbedtls_ssl_config config_;mbedtls_x509_crt ca_;
 mbedtls_entropy_context entropy_;mbedtls_ctr_drbg_context random_;
 bool initialized_=false;
 void Init();void Free();bool Guard();int Fail(int);void ClearWrite();
 static int Send(void*,const unsigned char*,size_t);static int Receive(void*,unsigned char*,size_t);
};
}
