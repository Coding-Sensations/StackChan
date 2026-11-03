#include "existing_sc_pair.hpp"
#include <nvs.h>
namespace companion {
StoreResult NvsExistingScPairBackend::Read(ExistingScPairRecord& out){
    ClearExistingRecord(out);nvs_handle_t h=0;auto r=nvs_open("sc_pair",NVS_READONLY,&h);
    if(r==ESP_ERR_NVS_NOT_FOUND)return StoreResult::Missing;
    if(r!=ESP_OK)return StoreResult::IoFailure;
    size_t n=0;r=nvs_get_blob(h,"record",nullptr,&n);
    if(r==ESP_OK&&n!=sizeof(out)){nvs_close(h);return StoreResult::Malformed;}
    if(r==ESP_OK)r=nvs_get_blob(h,"record",&out,&n);
    nvs_close(h);
    if(r==ESP_ERR_NVS_NOT_FOUND)return StoreResult::Missing;
    if(r!=ESP_OK||n!=sizeof(out)){ClearExistingRecord(out);return StoreResult::IoFailure;}
    return StoreResult::Ok;
}
StoreResult NvsExistingScPairBackend::Forget(){
    nvs_handle_t h=0;auto r=nvs_open("sc_pair",NVS_READWRITE,&h);
    if(r==ESP_ERR_NVS_NOT_FOUND)return StoreResult::Ok;
    if(r!=ESP_OK)return StoreResult::IoFailure;
    r=nvs_erase_key(h,"record");if(r==ESP_ERR_NVS_NOT_FOUND)r=ESP_OK;
    if(r==ESP_OK)r=nvs_commit(h);
    size_t n=0;if(r==ESP_OK&&nvs_get_blob(h,"record",nullptr,&n)!=ESP_ERR_NVS_NOT_FOUND)r=ESP_FAIL;
    nvs_close(h);return r==ESP_OK?StoreResult::Ok:StoreResult::IoFailure;
}
}
