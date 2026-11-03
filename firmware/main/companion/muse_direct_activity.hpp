// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <atomic>
namespace companion::muse {
// Pairing/preparation and direct chat share foreground custody. This is not an
// account, network or physical confirmation grant. Window retention owns it.
class ActivityCustody {
 static std::atomic<const void*>& Held(){static std::atomic<const void*> value{nullptr};return value;}
 bool held_=false;
public:
 ActivityCustody()=default;ActivityCustody(const ActivityCustody&)=delete;
 bool Acquire(){if(held_)return false;const void* empty=nullptr;held_=Held().compare_exchange_strong(empty,this);return held_;}
 ~ActivityCustody(){if(held_){const void* owned=this;Held().compare_exchange_strong(owned,nullptr);}}
};
}
