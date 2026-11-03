#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
using nvs_handle_t=uint32_t;
constexpr int ESP_OK=0,NVS_READONLY=0,NVS_READWRITE=1,ESP_ERR_NVS_NOT_FOUND=1;
extern std::map<std::string,std::string> legacy;
extern std::vector<uint8_t> committed,pending;
extern bool commit_failure;
extern int writes;
inline int nvs_open(const char* ns,int mode,nvs_handle_t* h){if(std::string(ns)!="wifi")return 1;*h=mode+1;return 0;}
inline void nvs_close(nvs_handle_t){}
inline int nvs_get_str(nvs_handle_t,const char* key,char* data,size_t* n){auto i=legacy.find(key);if(i==legacy.end()||*n<i->second.size()+1)return 1;*n=i->second.size()+1;std::memcpy(data,i->second.c_str(),*n);return 0;}
inline int nvs_get_blob(nvs_handle_t,const char* key,void* data,size_t* n){if(std::string(key)!="config_v1"||committed.empty())return 1;if(!data){*n=committed.size();return 0;}if(*n<committed.size())return 1;std::memcpy(data,committed.data(),committed.size());*n=committed.size();return 0;}
inline int nvs_set_blob(nvs_handle_t h,const char* key,const void* data,size_t n){if(h!=2||std::string(key)!="config_v1")return 1;++writes;auto p=static_cast<const uint8_t*>(data);pending.assign(p,p+n);return 0;}
inline int nvs_commit(nvs_handle_t){if(commit_failure)return 1;committed=pending;return 0;}

inline int nvs_set_str(nvs_handle_t,const char* key,const char* value){if(commit_failure)return 2;legacy[key]=value;++writes;return ESP_OK;}
inline int nvs_erase_key(nvs_handle_t,const char* key){if(commit_failure)return 2;return legacy.erase(key)?ESP_OK:ESP_ERR_NVS_NOT_FOUND;}
