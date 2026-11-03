// SPDX-License-Identifier: Apache-2.0
#include "muse_direct_api.h"
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <ctime>
namespace companion::muse {
namespace {
struct Response {
    std::string body;
    bool overflow=false;
    ~Response(){Wipe(body);}
};
esp_err_t Event(esp_http_client_event_t* event){
    if(event->event_id!=HTTP_EVENT_ON_DATA)return ESP_OK;
    auto* response=static_cast<Response*>(event->user_data);
    if(!response||event->data_len<0||(event->data_len&&!event->data)||response->body.size()+event->data_len>8192){if(response)response->overflow=true;return ESP_FAIL;}
    if(!event->data_len)return ESP_OK;
    response->body.append(static_cast<const char*>(event->data),event->data_len);return ESP_OK;
}
}
cJSON* DirectApiVerifier::Fetch(const DeviceRecord& record){
    if(!current_||!current_()||!record.access.Size()||std::time(nullptr)<1735689600)return nullptr;
    Response response;esp_http_client_config_t config{};
    config.url="https://api.muse.ai/fetch_vms";config.transport_type=HTTP_TRANSPORT_OVER_SSL;
    config.crt_bundle_attach=esp_crt_bundle_attach;config.tls_version=ESP_HTTP_CLIENT_TLS_VER_TLS_1_2;
    config.disable_auto_redirect=true;config.timeout_ms=5000;config.keep_alive_enable=false;
    config.event_handler=Event;config.user_data=&response;
    auto client=esp_http_client_init(&config);if(!client)return nullptr;
    std::string bearer="Bearer ";bearer+=record.access.Get();
    const bool headers=esp_http_client_set_header(client,"Authorization",bearer.c_str())==ESP_OK&&esp_http_client_set_header(client,"X-API-Version","1.0.0")==ESP_OK;
    Wipe(bearer);
    const auto result=headers&&current_()?esp_http_client_perform(client):ESP_FAIL;
    const int status=esp_http_client_get_status_code(client);esp_http_client_cleanup(client);
    if(result!=ESP_OK||status!=200||response.overflow||!current_())return nullptr;
    Json root(Parse(response.body));if(!Unique(root.value)||cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root.value,"error"))||!Text(root.value,"error_title").empty())return nullptr;
    auto* value=root.value;root.value=nullptr;return value;
}
bool DirectApiVerifier::Verify(const DeviceRecord& record){
    Json root(Fetch(record));auto* list=cJSON_GetObjectItemCaseSensitive(root.value,"vm_list");
    if(!cJSON_IsArray(list)||!cJSON_GetArraySize(list))return false;
    for(auto* item=list->child;item;item=item->next){
        if(!Unique(item))return false;
        const auto* token=cJSON_GetObjectItemCaseSensitive(item,"vm_auth_token");
        const auto* url=cJSON_GetObjectItemCaseSensitive(item,"vm_ws_url");
        if(!cJSON_IsString(url))url=cJSON_GetObjectItemCaseSensitive(item,"vm_url");
        if(cJSON_IsString(token)&&token->valuestring&&token->valuestring[0]&&std::strlen(token->valuestring)<=3072&&cJSON_IsString(url)&&url->valuestring&&url->valuestring[0])return true;
    }
    return false;
}
bool DirectApiVerifier::FindExactVm(const DeviceRecord& record,const std::string& vm,VmCredential& output){
    output.Clear();if(!Id(vm))return false;
    Json root(Fetch(record));auto* list=cJSON_GetObjectItemCaseSensitive(root.value,"vm_list");if(!cJSON_IsArray(list))return false;
    if(!SelectExactVm(root.value,vm,output)||!current_()){output.Clear();return false;}return true;
}
}
