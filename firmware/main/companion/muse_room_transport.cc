#include "muse_room_transport.h"
#include "muse_room_peer.hpp"
#include "bounded_web_socket.h"
#include "local_tls.h"
#include <esp_timer.h>
#include <memory>
#include <mutex>

namespace companion::room {
namespace {
class Transport {
  std::mutex mutex_;
  std::shared_ptr<BoundedWebSocket> socket_;
  MuseRoomPeer peer_;
  bool stopping_=false;
  TaskHandle_t owner_=nullptr;
  std::function<bool()> network_current_;
  static uint64_t Now(){return uint64_t(esp_timer_get_time()/1000);}
public:
  bool Start(NetworkInterface& network,const MuseRoomGrant& grant) {
    const char* expected=companion_local_tls_host();const char* ca=companion_local_ca_pem();
    if(!expected||!ca||!*ca||!grant.network_current||!grant.network_current()||!MuseRoomUri(grant.uri,expected))return false;
    {std::lock_guard<std::mutex> lock(mutex_);if((owner_&&owner_!=xTaskGetCurrentTaskHandle())||socket_||stopping_||!peer_.Open(grant,Now()))return false;owner_=xTaskGetCurrentTaskHandle();network_current_=grant.network_current;}
    // Connection 3 uses the existing joined local TLS path without borrowing
    // Mira's audio socket/codec or starting a network owner. No new trust pin.
    auto socket=std::make_shared<BoundedWebSocket>(&network,3);
    socket->SetReceiveBufferSize(2048);
    socket->OnData([this](const char* data,size_t size,bool binary){
      std::string reply;std::shared_ptr<BoundedWebSocket> current;
      {std::lock_guard<std::mutex> lock(mutex_);
       if(stopping_||!socket_)return;
       if(!network_current_||!network_current_()){peer_.Close();stopping_=true;return;}
       if(!data||!size||size>2048||!peer_.Receive(std::string(data,size),binary,Now(),reply)){stopping_=true;return;}
       current=socket_;}
      if(!reply.empty()&&!current->Send(reply)){std::lock_guard<std::mutex> lock(mutex_);peer_.Close();stopping_=true;}
    });
    auto fail=[this]{std::lock_guard<std::mutex> lock(mutex_);peer_.Close();stopping_=true;};
    socket->OnDisconnected(fail);socket->OnError([fail](int){fail();});
    {std::lock_guard<std::mutex> lock(mutex_);socket_=socket;}
    if(!socket->Connect(grant.uri.c_str())){Stop();return false;}
    if(!grant.network_current()){Stop();return false;}return true;
  }
  void Poll(){bool stop=false;{std::lock_guard<std::mutex> lock(mutex_);if(socket_&&owner_==xTaskGetCurrentTaskHandle()){if(!network_current_||!network_current_()||!peer_.Tick(Now())){peer_.Close();stopping_=true;}stop=stopping_;}}if(stop)Stop();}
  void Stop(){std::shared_ptr<BoundedWebSocket> old;{std::lock_guard<std::mutex> lock(mutex_);stopping_=true;peer_.Close();if(owner_&&owner_!=xTaskGetCurrentTaskHandle())return;old=std::move(socket_);}if(old)old->Close();{std::lock_guard<std::mutex> lock(mutex_);stopping_=false;owner_=nullptr;network_current_={};}}
  bool CleanupComplete(){std::lock_guard<std::mutex> lock(mutex_);return !socket_&&!stopping_;}
  bool Authenticated(){std::lock_guard<std::mutex> lock(mutex_);return peer_.Authenticated();}
};
Transport transport;
std::mutex preparation_mutex;
std::unique_ptr<MuseRoomGrant> preparation;
NetworkInterface* prepared_network=nullptr;
void ClearPreparation(){if(preparation){for(volatile auto& c:preparation->event)c=0;for(volatile auto& c:preparation->control)c=0;}preparation.reset();prepared_network=nullptr;}
}
bool StartMuseRoom(NetworkInterface& network,const MuseRoomGrant& grant){return transport.Start(network,grant);}
bool PrepareMuseRoom(NetworkInterface& network,const MuseRoomGrant& grant){
  const char* host=companion_local_tls_host();const char* ca=companion_local_ca_pem();
  if(!host||!ca||!*ca||!grant.network_current||!grant.network_current()||!MuseRoomUri(grant.uri,host)||!Id(grant.binding)||!Id(grant.generation)||
     grant.event_size<32||grant.event_size>64||grant.control_size<32||grant.control_size>64||
     grant.expires_ms<=uint64_t(esp_timer_get_time()/1000)||
     (grant.event_size==grant.control_size&&!std::memcmp(grant.event.data(),grant.control.data(),grant.event_size))||
     !ProviderSelected(Provider::Muse)||!transport.CleanupComplete())return false;
  std::lock_guard<std::mutex> lock(preparation_mutex);if(preparation)return false;
  preparation=std::make_unique<MuseRoomGrant>(grant);prepared_network=&network;return true;
}
bool MuseRoomPrepared(){std::lock_guard<std::mutex> lock(preparation_mutex);if(preparation&&(!ProviderSelected(Provider::Muse)||preparation->expires_ms<=uint64_t(esp_timer_get_time()/1000)||!preparation->network_current||!preparation->network_current()))ClearPreparation();return bool(preparation);}
bool ConnectPreparedMuseRoom(){std::lock_guard<std::mutex> lock(preparation_mutex);if(!preparation||!prepared_network||!ProviderSelected(Provider::Muse)){ClearPreparation();return false;}const bool result=transport.Start(*prepared_network,*preparation);ClearPreparation();return result;}
void PollMuseRoom(){transport.Poll();}
void StopMuseRoom(){transport.Stop();std::lock_guard<std::mutex> lock(preparation_mutex);ClearPreparation();}
bool MuseRoomAuthenticated(){return transport.Authenticated();}
bool MuseRoomCleanupComplete(){return transport.CleanupComplete();}
}
