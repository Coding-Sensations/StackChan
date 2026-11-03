#pragma once
#include <cstdint>
#include <cstddef>
#include <cassert>
using nvs_handle_t=uint32_t;
constexpr int ESP_OK=0, ESP_ERR_NVS_NOT_FOUND=1, NVS_READONLY=0,NVS_READWRITE=1;
extern int writes,commits,opens,closes,last_mode;
#define ESP_ERROR_CHECK(x) assert((x)==0)
inline int nvs_open(const char*,int mode,nvs_handle_t* h){++opens;last_mode=mode;*h=1;return 0;}
inline void nvs_close(nvs_handle_t){++closes;}
inline int nvs_commit(nvs_handle_t){++commits;return 0;}
inline int nvs_get_str(nvs_handle_t,const char*,char*,size_t*){return 1;}
inline int nvs_get_i32(nvs_handle_t,const char*,int32_t*){return 1;}
inline int nvs_get_u8(nvs_handle_t,const char*,uint8_t*){return 1;}
inline int nvs_set_str(nvs_handle_t,const char*,const char*){++writes;return 0;}
inline int nvs_set_i32(nvs_handle_t,const char*,int32_t){++writes;return 0;}
inline int nvs_set_u8(nvs_handle_t,const char*,uint8_t){++writes;return 0;}
inline int nvs_erase_key(nvs_handle_t,const char*){++writes;return 0;}
inline int nvs_erase_all(nvs_handle_t){++writes;return 0;}
