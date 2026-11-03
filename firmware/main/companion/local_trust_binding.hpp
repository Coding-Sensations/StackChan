#pragma once
#include "personal_binding.hpp"
extern "C" const char* companion_local_ca_pem();
extern "C" const char* companion_local_tls_host();
#include <mbedtls/sha256.h>
#include <cstring>
namespace companion {
inline bool LocalTrustBindingMatches() {
 if(!PersonalBindingAvailable())return false;
 const char* ca=companion_local_ca_pem();const char* host=companion_local_tls_host();
 if(!ca||!*ca||!host||std::strcmp(host,STACKCHAN_PIN_HOST))return false;
 unsigned char digest[32];if(mbedtls_sha256(reinterpret_cast<const unsigned char*>(ca),std::strlen(ca),digest,0))return false;
 const char* hex="0123456789abcdef";char hash[65];
 for(unsigned i=0;i<32;++i){hash[2*i]=hex[digest[i]>>4];hash[2*i+1]=hex[digest[i]&15];}hash[64]=0;
 return std::strcmp(hash,STACKCHAN_PIN_CA_SHA256)==0;
}
}
