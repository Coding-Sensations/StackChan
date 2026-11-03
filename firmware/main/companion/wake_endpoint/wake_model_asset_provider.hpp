#pragma once
#include <cstddef>
#include <cstdint>
namespace companion::wake_runtime {
struct WakeModelAssetView final {
    const std::uint8_t* bytes=nullptr;std::size_t size=0;bool qualification=false;
};
// Source-owned absence is concrete and linkable, not an undefined provider hook.
// The separately frozen ten-step trial remains an offline engineering asset;
// weights are not embedded/selected/installed by this inactive firmware source.
inline WakeModelAssetView QualifiedAssetView()noexcept{return {};}
}
