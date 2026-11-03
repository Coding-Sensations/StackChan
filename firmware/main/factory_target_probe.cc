#include "companion/wake_endpoint/psram_endpoint_factory.hpp"
#include <type_traits>
static_assert(!std::is_constructible<companion::wake_runtime::DetachedCpuEndpointReceipt,companion::wake_runtime::BorrowedCpuEndpoint*,std::uint64_t>::value,"opaque producer receipt is not caller-constructible");
// No automatic model/frontend/endpoint object. Link-only supplied owner handle;
// the default unavailable factory cannot allocate or install a borrower.
extern "C" bool MiraWakeFactoryLinkReference(
    companion::wake_runtime::FactoryHandle& owner,
    const companion::wake_runtime::DetachedCpuEndpointReceipt& detached,
    companion::wake_runtime::ConsentAuthority& authority,
    companion::wake_runtime::ClockFn clock,void* context,std::uint64_t generation)noexcept{
    const bool unavailable=!owner.CreateQualified(generation,authority,clock,context);
    return unavailable&&owner.DestroyDetached(detached);
}
