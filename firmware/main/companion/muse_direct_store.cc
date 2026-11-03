#include "muse_direct_credential_policy.hpp"
#if !STACKCHAN_MUSE_RAM_ONLY
// SPDX-License-Identifier: Apache-2.0
#include "muse_direct_store.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <mutex>
#include <memory>
#include <new>
namespace companion::muse {
namespace {std::mutex& StoreMutex(){static std::mutex mutex;return mutex;}}
bool DirectNvsStore::CanCreateNew(){
    std::lock_guard<std::mutex> lock(StoreMutex());
    nvs_stats_t stats{};if(nvs_get_stats("nvs",&stats)!=ESP_OK||stats.free_entries<136)return false;
    nvs_handle_t handle=0;const auto opened=nvs_open("muse_direct",NVS_READONLY,&handle);
    if(opened==ESP_ERR_NVS_NOT_FOUND)return true;
    if(opened!=ESP_OK)return false;
    size_t length=0;const auto found=nvs_get_blob(handle,"device_v1",nullptr,&length);nvs_close(handle);
    return found==ESP_ERR_NVS_NOT_FOUND;
}
StoreResult DirectNvsStore::CommitNew(const DeviceRecord& record){
    std::lock_guard<std::mutex> lock(StoreMutex());
    if(!Id(record.node)||!record.access.Size()||!record.refresh.Size())return StoreResult::NoWrite;
    Json root(cJSON_CreateObject());if(!root.value)return StoreResult::NoWrite;
    cJSON_AddNumberToObject(root.value,"version",1);cJSON_AddStringToObject(root.value,"node",record.node.c_str());
    cJSON_AddStringToObject(root.value,"access_token",record.access.Get());cJSON_AddStringToObject(root.value,"refresh_token",record.refresh.Get());
    if(!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(root.value,"access_token"))||!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(root.value,"refresh_token")))return StoreResult::NoWrite;
    auto body=Encode(root.value);if(body.empty()||body.size()>4096){Wipe(body);return StoreResult::NoWrite;}
    nvs_stats_t stats{};if(nvs_get_stats("nvs",&stats)!=ESP_OK||stats.free_entries<(body.size()+31)/32+8){Wipe(body);return StoreResult::NoWrite;}
    nvs_handle_t handle=0;
    if(nvs_open("muse_direct",NVS_READWRITE,&handle)!=ESP_OK){Wipe(body);return StoreResult::NoWrite;}
    size_t existing=0;const auto found=nvs_get_blob(handle,"device_v1",nullptr,&existing);
    if(found!=ESP_ERR_NVS_NOT_FOUND){nvs_close(handle);Wipe(body);return StoreResult::NoWrite;}
    // One atomic blob, no overwrite. Even a failed set/commit may have persisted
    // the new value: report uncertainty and never automatically retry/erase it.
    const auto written=nvs_set_blob(handle,"device_v1",body.c_str(),body.size()+1);
    const auto committed=written==ESP_OK?nvs_commit(handle):written;
    nvs_close(handle);Wipe(body);
    return written==ESP_OK&&committed==ESP_OK?StoreResult::Committed:StoreResult::Uncertain;
}
bool DirectNvsStore::LoadAccess(const std::string& node,std::function<bool()> current,DeviceRecord& output){
    output.Clear();if(!Id(node)||!current||!current())return false;
    std::lock_guard<std::mutex> lock(StoreMutex());if(!current())return false;
    nvs_handle_t handle=0;if(nvs_open("muse_direct",NVS_READONLY,&handle)!=ESP_OK)return false;
    size_t length=0;const auto sized=nvs_get_blob(handle,"device_v1",nullptr,&length);
    if(sized!=ESP_OK||length<2||length>4097||!current()){nvs_close(handle);return false;}
    struct Blob{std::array<char,4097> data{};~Blob(){for(volatile char& c:data)c=0;}};
    std::unique_ptr<Blob> blob(new(std::nothrow)Blob());if(!blob){nvs_close(handle);return false;}
    if(!current()){nvs_close(handle);return false;}
    const size_t expected=length;const auto read=nvs_get_blob(handle,"device_v1",blob->data.data(),&length);nvs_close(handle);
    if(read!=ESP_OK||length!=expected||!current()||blob->data[length-1]!=0||std::memchr(blob->data.data(),0,length-1))return false;
    Json root(Parse(std::string_view(blob->data.data(),length-1)));
    auto* version=cJSON_GetObjectItemCaseSensitive(root.value,"version");auto* access=cJSON_GetObjectItemCaseSensitive(root.value,"access_token");
    if(!Unique(root.value)||cJSON_GetArraySize(root.value)!=4||!cJSON_IsNumber(version)||version->valuedouble!=1||Text(root.value,"node")!=node||!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(root.value,"refresh_token"))||!cJSON_IsString(access)||!access->valuestring||!current()||!output.access.Set(access->valuestring))return false;
    // The refresh token is parsed only as part of the atomic blob, then wiped;
    // it is never copied into the runtime credential or used for a refresh.
    output.node=node;if(!current()){output.Clear();return false;}return true;
}
}

#endif
