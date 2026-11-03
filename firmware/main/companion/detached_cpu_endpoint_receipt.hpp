#pragma once
#include <cstdint>
namespace companion {class CodecTasks;namespace wake_runtime {
class BorrowedCpuEndpoint;class LocalWakeHardwareOwner;
// Snapshot proves a pointer was detached under producer admission serialization,
// not merely that a Stop request was made. A later Start cannot use this pointer.
class DetachedCpuEndpointReceipt {
    BorrowedCpuEndpoint* endpoint_=nullptr;uint64_t generation_=0;
    friend class ::companion::CodecTasks;friend class LocalWakeHardwareOwner;
    DetachedCpuEndpointReceipt(BorrowedCpuEndpoint* endpoint,uint64_t generation):endpoint_(endpoint),generation_(generation){}
public:
    DetachedCpuEndpointReceipt()=default;
    BorrowedCpuEndpoint* Endpoint()const noexcept{return endpoint_;}
    uint64_t OwnerGeneration()const noexcept{return generation_;}
    explicit operator bool()const noexcept{return endpoint_&&generation_;}
};
}}
