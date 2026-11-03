#pragma once
#include <cstdint>
namespace companion::wake_runtime {
// Compiling baseline with no storage behavior, for meaningful semantic-red
// evidence. Never used in target/production selection.
template<class Endpoint,class Allocator>class OwnedEndpointStorage final {
public:
 Endpoint* EndpointPointer()const noexcept{return nullptr;}
 std::uint64_t OwnerGeneration()const noexcept{return 0;}
 template<class...Args>bool ConstructInert(std::uint64_t,Args&&...)noexcept{return false;}
 bool DestroyAfterProducerStop(bool,std::uint64_t)noexcept{return false;}
};
}
