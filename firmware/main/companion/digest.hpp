#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#if defined(ESP_PLATFORM)
#include "mbedtls/md.h"
#elif defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#else
#error Existing audited SHA256 provider required
#endif
namespace mira_model {
inline std::array<std::uint8_t,32> Sha256(const std::uint8_t* data,std::size_t size) noexcept {
 std::array<std::uint8_t,32> result{};
#if defined(ESP_PLATFORM)
 const mbedtls_md_info_t* md=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
 if(!md||mbedtls_md(md,data,size,result.data())!=0)result.fill(0);
#else
 if(size>0xffffffffU || CC_SHA256(data,static_cast<CC_LONG>(size),result.data())==nullptr)result.fill(0);
#endif
 return result;
}
}
