#pragma once
#include "owned_endpoint_storage.hpp"
#include "endpoint_generation_fence.hpp"
#include <companion/checked_native_endpoint.hpp>
#include <companion/detached_cpu_endpoint_receipt.hpp>
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
namespace companion::wake_runtime {
inline constexpr bool kQualifiedEndpointFactoryAvailable=false;
struct ExplicitPsramAllocator final {
    static void* Allocate(std::size_t alignment,std::size_t bytes)noexcept{
        // No internal DRAM fallback and no task-stack or static large object.
        return heap_caps_aligned_alloc(alignment,bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    }
    static bool IsExternal(const void* p)noexcept{return esp_ptr_external_ram(p);}
    static void Free(void* p)noexcept{heap_caps_free(p);}
};
class FactoryHandle final {
    OwnedEndpointStorage<CheckedNativeEndpoint,ExplicitPsramAllocator> storage_;
public:
    FactoryHandle()=default;FactoryHandle(const FactoryHandle&)=delete;
    FactoryHandle& operator=(const FactoryHandle&)=delete;
    BorrowedCpuEndpoint* Endpoint()const noexcept{return storage_.EndpointPointer();}
    std::uint64_t OwnerGeneration()const noexcept{return storage_.OwnerGeneration();}
    // Explicit future qualified lifecycle only. Today this returns false before
    // allocation; no default asset/provider/availability and no grant issuer.
    // Authority, trusted clock/context and immutable asset must outlive checked
    // producer Stop and successful CPU teardown. Generation is custody context.
    bool CreateQualified(std::uint64_t generation,ConsentAuthority& authority,ClockFn clock,void* context,const std::uint8_t* asset=nullptr,std::size_t bytes=0)noexcept{
        if(storage_.EndpointPointer()||!kQualifiedEndpointFactoryAvailable||!mira_model::RuntimeAvailable||!kNativePortsLiveAvailable||
           !kLiveBorrowedProcessingAvailable||!clock||!asset||bytes!=mira_model::ModelBytes)return false;
        // This branch is compiled/type-checked but cannot be entered with current
        // gates. Qualification must include model hash/quality and dispatcher.
        if(!EndpointGenerationFence::Claim(generation))return false;
        return storage_.ConstructInert(generation,authority,clock,context,asset,bytes);
    }
    // Receipt is issued by the real serialized producer ONLY after checked Stop
    // and detaching this exact pointer. CPU cleanup never releases hardware IO.
    bool DestroyDetached(const DetachedCpuEndpointReceipt& receipt)noexcept{
        if(!storage_.EndpointPointer())return true;
        if(!receipt||receipt.Endpoint()!=storage_.EndpointPointer()||receipt.OwnerGeneration()!=storage_.OwnerGeneration())return false;
        return storage_.DestroyAfterProducerStop(true,receipt.OwnerGeneration());
    }
};
static_assert(alignof(CheckedNativeEndpoint)<=16,"PSRAM endpoint alignment");
static_assert(sizeof(FactoryHandle)<=32,"owner handle stays small");
}
