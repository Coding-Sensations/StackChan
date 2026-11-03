#pragma once
#include <array>
#include <string>
#include <esp_random.h>
namespace companion::desk {
inline const std::array<uint8_t,16>& CameraProducerBootBytes(){
    // Lazy boot identity: no camera init, stream, model or device permission.
    static const auto boot=[](){std::array<uint8_t,16> b{};esp_fill_random(b.data(),b.size());return b;}();return boot;
}
inline std::string CameraProducerBootHex(){
    static const char h[]="0123456789abcdef";std::string out;
    for(auto b:CameraProducerBootBytes()){out+=h[b>>4];out+=h[b&15];}return out;
}
}
