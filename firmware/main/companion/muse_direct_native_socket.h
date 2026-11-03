// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_socket.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
struct esp_tls;
namespace companion::muse {
enum class OfficialEndpoint {Meta,Api};
// Retain this object and its grant callback after a false CloseAndJoin receipt.
// A DNS worker can outlive the connect budget; it holds no token or socket.
class NativeMetaStream final:public MetaByteStream {
 std::function<bool()> current_;
 OfficialEndpoint endpoint_;
 const char* Host()const{return endpoint_==OfficialEndpoint::Api?"api.muse.ai":MetaHost;}
 TaskHandle_t owner_=nullptr,resolver_=nullptr;
 std::atomic<bool> revoked_{false},resolver_done_{true};
 bool spent_=false,verified_=false,clean_=true,resolved_=false,close_uncertain_=false;
 char numeric_[16]{};
 esp_tls* tls_=nullptr;
 static void Resolve(void*);
 bool Current();bool JoinResolver(unsigned timeout_ms);int Io(uint8_t*,const uint8_t*,size_t,unsigned);
public:
 explicit NativeMetaStream(std::function<bool()> current,OfficialEndpoint endpoint=OfficialEndpoint::Meta):current_(std::move(current)),endpoint_(endpoint){}
 bool ConnectFixedMeta(unsigned timeout_ms)override;
 bool PeerVerified()const override;
 int Read(uint8_t*,size_t,unsigned timeout_ms)override;
 int Write(const uint8_t*,size_t,unsigned timeout_ms)override;
 bool CloseAndJoin(unsigned timeout_ms)override;
 bool CleanupComplete()const{return clean_;}
};
class NativeWebSocketEntropy final:public WebSocketEntropy {
public:
 bool Nonce(char key[25],char expected_accept[29])override;
 bool Mask(uint8_t out[4])override;
};
class DirectMetaSocket final:public NoiseSocket {
 NativeMetaStream stream_;NativeWebSocketEntropy entropy_;MetaWebSocket socket_;
public:
 explicit DirectMetaSocket(std::function<bool()> current);
 bool ConnectMeta(const std::string& vm,const char* bearer,unsigned timeout)override{return socket_.ConnectMeta(vm,bearer,timeout);}
 bool VerifiedMetaEndpoint()const override{return socket_.VerifiedMetaEndpoint();}
 bool Send(const uint8_t* data,size_t size,unsigned timeout)override{return socket_.Send(data,size,timeout);}
 int Receive(uint8_t* data,size_t size,unsigned timeout)override{return socket_.Receive(data,size,timeout);}
 bool CloseAndJoin(unsigned timeout)override{return socket_.CloseAndJoin(timeout);}
};
}
