#pragma once
#include "local_consent.hpp"
#include <cstddef>
namespace companion::wake_runtime {
// Optional software borrower installed only while its producer is stopped.
// Producer owns IO permission and hardware close/join; endpoint owns CPU cleanup.
// Calls all run on the producer worker; endpoint must outlive checked producer Stop.
class BorrowedCpuEndpoint {
public:
    virtual ~BorrowedCpuEndpoint()=default;
    virtual bool AcceptPhysicalProof(AudioLease,uint32_t,ExplicitLocalConsent)noexcept=0;
    virtual bool Feed16k(AudioLease,uint32_t,const int16_t*,size_t)noexcept=0;
    virtual bool CloseCpu()noexcept=0;
};
}
