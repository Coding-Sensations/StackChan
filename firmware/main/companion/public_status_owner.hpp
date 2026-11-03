#pragma once
namespace companion {
// Read only, called on the same app loop as OpenView/TickView/onClose.
// The wrapper clears its registration before destroying the owner.
class PublicStatusOwnerProbe {
 void* owner_=nullptr;bool (*idle_)(void*)=nullptr;
public:
 void Set(void* owner,bool (*idle)(void*)){owner_=owner;idle_=idle;}
 void Clear(void* owner){if(owner_==owner){owner_=nullptr;idle_=nullptr;}}
 bool Idle()const{return !idle_||idle_(owner_);}
};
inline PublicStatusOwnerProbe& PublicStatusOwner(){static PublicStatusOwnerProbe probe;return probe;}
}
