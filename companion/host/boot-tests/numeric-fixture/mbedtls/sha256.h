#pragma once
// Deterministic SHA boundary for this exact synthetic parser fixture only.
// Production cryptography is separately compiled against the actual SDK.
#include <cstring>
#include <cstddef>
inline int mbedtls_sha256(const unsigned char* input, std::size_t size, unsigned char* output, int is224){
 const char ca[]="synthetic-CA-no-authority";
 if(is224 || size!=sizeof(ca)-1 || std::memcmp(input,ca,size))return -1;
 const unsigned char digest[]={62,68,2,33,192,70,202,176,63,61,49,219,143,79,64,195,125,107,6,222,145,92,196,161,46,18,47,211,144,173,99,133};
 std::memcpy(output,digest,32);return 0;
}
