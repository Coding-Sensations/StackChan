// SPDX-License-Identifier: Apache-2.0
#include "exchange_api.hpp"
#include <new>
namespace {
using voice_prep::ServiceExchange;
ServiceExchange* Construct(void* storage,size_t size,voice_prep::Protocol* protocol,musegadgets::noise::core::ClientSession* session){
 if(!storage||size<sizeof(ServiceExchange)||reinterpret_cast<uintptr_t>(storage)%alignof(ServiceExchange)||!protocol||!session)return nullptr;
 return ::new(storage) ServiceExchange(*protocol,*session);
}
bool Destroy(ServiceExchange* owner){
 if(!owner)return true;
 if(!owner->CleanupComplete())return false; // unknown join retains storage
 owner->~ServiceExchange();return true;
}
constexpr voice_prep::ExchangeApi api{1,sizeof(ServiceExchange),alignof(ServiceExchange),Construct,Destroy,
 &ServiceExchange::Begin,&ServiceExchange::Queue,&ServiceExchange::Next,&ServiceExchange::Written,&ServiceExchange::Receive,
 &ServiceExchange::Cancel,&ServiceExchange::Abort,&ServiceExchange::Joined,&ServiceExchange::NeedsTransportClose,&ServiceExchange::CleanupComplete};
}
// Dormant function table only; never constructs a session, worker or permission.
extern "C" const voice_prep::ExchangeApi* stackchan_muse_voice_exchange_api(){return &api;}
