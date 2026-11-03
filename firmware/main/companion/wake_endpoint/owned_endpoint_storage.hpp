#pragma once
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
namespace companion::wake_runtime {
// Memory custody, not permission or physical-Stop attestation. Exclusive owner
// thread; storage/provider lifetime spans every synchronous endpoint callback.
// The caller passes joined=true only AFTER its checked producer Stop has joined.
// A retained pointer is never overwritten or freed on failed CPU cleanup.
template<class Endpoint,class Allocator> class OwnedEndpointStorage final {
    static_assert(alignof(Endpoint)<=16,"bounded endpoint alignment");
    Endpoint* endpoint_=nullptr;std::uint64_t owner_generation_=0,last_generation_=0;
    static void Wipe(void* block)noexcept{volatile unsigned char* p=static_cast<volatile unsigned char*>(block);for(std::size_t i=0;i<sizeof(Endpoint);++i)p[i]=0;}
public:
    OwnedEndpointStorage()=default;
    OwnedEndpointStorage(const OwnedEndpointStorage&)=delete;
    OwnedEndpointStorage& operator=(const OwnedEndpointStorage&)=delete;
    // No automatic destruction/free: producer could still hold the borrower.
    // Owner must complete DestroyAfterProducerStop before its own destruction.
    Endpoint* EndpointPointer()const noexcept{return endpoint_;}
    std::uint64_t OwnerGeneration()const noexcept{return owner_generation_;}
    template<class...Args>bool ConstructInert(std::uint64_t generation,Args&&... args)noexcept{
        if(endpoint_||!generation||generation<=last_generation_)return false;
        void* p=Allocator::Allocate(16,sizeof(Endpoint));
        if(!p)return false;
        if(reinterpret_cast<std::uintptr_t>(p)%16 || !Allocator::IsExternal(p)) {Wipe(p);Allocator::Free(p);return false;}
        endpoint_=new(p)Endpoint(static_cast<Args&&>(args)...);owner_generation_=last_generation_=generation;return true;
    }
    bool DestroyAfterProducerStop(bool joined,std::uint64_t generation)noexcept{
        if(!endpoint_)return true;
        if(!joined||!generation||generation!=owner_generation_||!endpoint_->CloseCpu())return false;
        void* p=endpoint_;endpoint_->~Endpoint();Wipe(p);Allocator::Free(p);
        endpoint_=nullptr;owner_generation_=0;return true;
    }
};
}
