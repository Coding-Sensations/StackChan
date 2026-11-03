#pragma once
#include <cstddef>
inline void esp_fill_random(void* p,size_t n){auto b=static_cast<unsigned char*>(p);for(size_t i=0;i<n;++i)b[i]=static_cast<unsigned char>(i+1);}
