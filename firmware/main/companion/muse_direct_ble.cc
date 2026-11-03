// SPDX-License-Identifier: Apache-2.0
#include "muse_direct_ble.h"
extern "C" {
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
}
#include <algorithm>
#include <cstring>
namespace companion::muse {
namespace {
std::atomic<DirectBle*> active{nullptr};
const ble_uuid128_t service=BLE_UUID128_INIT(0x0c,0x24,0x5f,0xcf,0x4e,0x31,0x46,0x8b,0xcf,0x46,0xea,0x38,0x1c,0x3d,0xdd,0x7f);
const ble_uuid128_t rx=BLE_UUID128_INIT(0x01,0x9b,0x8f,0x5e,0x2d,0x3c,0xf0,0xa1,0x6e,0x4a,0xa2,0x28,0x29,0x30,0x59,0x4d);
const ble_uuid128_t tx=BLE_UUID128_INIT(0x6c,0x5b,0x4a,0x3f,0x2e,0x1d,0x0a,0x8f,0x9c,0x4e,0x2b,0x7b,0xca,0xc4,0x5d,0xd7);
uint16_t tx_value=0;
ble_gatt_chr_def chars[3]{};
ble_gatt_svc_def services[2]{};
}
bool DirectBle::Current()const{return !closing_&&CommonBleAuthority().Current(lease_)&&current_&&current_();}
void DirectBle::Advertise(){
    if(!Current()||connection_!=BLE_HS_CONN_HANDLE_NONE)return;
    ble_hs_adv_fields fields{};fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128=const_cast<ble_uuid128_t*>(&service);fields.num_uuids128=1;fields.uuids128_is_complete=1;
    if(ble_gap_adv_set_fields(&fields)){Reset(0);return;}
    ble_hs_adv_fields response{};response.name=reinterpret_cast<const uint8_t*>(name_.data());response.name_len=name_.size();response.name_is_complete=1;
    // SDK community-device paired flag, not an authority or a credential.
    const uint8_t manufacturer[]={0xff,0xff,0};response.mfg_data=manufacturer;response.mfg_data_len=sizeof(manufacturer);
    if(ble_gap_adv_rsp_set_fields(&response)){Reset(0);return;}
    ble_gap_adv_params params{};params.conn_mode=BLE_GAP_CONN_MODE_UND;params.disc_mode=BLE_GAP_DISC_MODE_GEN;
    if(ble_gap_adv_start(address_type_,nullptr,BLE_HS_FOREVER,&params,Gap,nullptr))Reset(0);
}
void DirectBle::Sync(){auto* p=active.load();if(!p||!p->Current())return;if(ble_hs_util_ensure_addr(0)||ble_hs_id_infer_auto(0,&p->address_type_)){Reset(0);return;}p->Advertise();}
void DirectBle::Reset(int){auto* p=active.load();if(p){p->closing_=true;CommonBleAuthority().Revoke(p->lease_);}}
int DirectBle::Gap(ble_gap_event* event,void*){
    auto* p=active.load();if(!p)return 0;
    switch(event->type){
    case BLE_GAP_EVENT_CONNECT:
        if(event->connect.status){p->Advertise();break;}
        if(!p->Current()||p->connection_!=BLE_HS_CONN_HANDLE_NONE){ble_gap_terminate(event->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM);break;}
        p->connection_=event->connect.conn_handle;p->subscribed_=false;break;
    case BLE_GAP_EVENT_DISCONNECT:
        if(event->disconnect.conn.conn_handle!=p->connection_)break;
        p->connection_=BLE_HS_CONN_HANDLE_NONE;p->subscribed_=false;
        if(p->Current()&&p->disconnected_)p->disconnected_();
        p->Advertise();break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if(event->subscribe.conn_handle==p->connection_&&event->subscribe.attr_handle==tx_value)p->subscribed_=event->subscribe.cur_notify;
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:p->Advertise();break;
    default:break;
    }
    return 0;
}
int DirectBle::Access(uint16_t connection,uint16_t,ble_gatt_access_ctxt* context,void*){
    auto* p=active.load();if(!p||!p->Current()||connection!=p->connection_)return BLE_ATT_ERR_UNLIKELY;
    if(context->op==BLE_GATT_ACCESS_OP_READ_CHR){const char status[]="idle";return os_mbuf_append(context->om,status,4)?BLE_ATT_ERR_INSUFFICIENT_RES:0;}
    if(context->op!=BLE_GATT_ACCESS_OP_WRITE_CHR||!p->subscribed_)return BLE_ATT_ERR_UNLIKELY;
    const auto length=OS_MBUF_PKTLEN(context->om);if(!length||length>160)return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    uint8_t packet[160];uint16_t copied=0;
    if(ble_hs_mbuf_to_flat(context->om,packet,sizeof(packet),&copied)||copied!=length)return BLE_ATT_ERR_UNLIKELY;
    if(p->receive_)p->receive_(packet,copied);
    return 0;
}
void DirectBle::Host(void* context){auto* p=static_cast<DirectBle*>(context);nimble_port_run();p->exited_=true;for(;;)vTaskSuspend(nullptr);}
bool DirectBle::Start(const std::string& name,std::function<bool()> current,std::function<void(const uint8_t*,size_t)> receive,std::function<void()> disconnected){
    if(lease_||name.empty()||name.size()>24||!current||!current()||!receive||!disconnected)return false;
    for(char c:name)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;
    if(!CommonBleAuthority().Acquire(BleOwner::Muse,lease_))return false;
    clean_=false;owner_=xTaskGetCurrentTaskHandle();current_=std::move(current);receive_=std::move(receive);disconnected_=std::move(disconnected);name_=name;closing_=false;
    DirectBle* empty=nullptr;if(!active.compare_exchange_strong(empty,this)){CommonBleAuthority().Release(lease_,true);clean_=true;return false;}
    if(nimble_port_init()!=ESP_OK){closing_=true;CommonBleAuthority().Revoke(lease_);return false;}
    initialized_=true;ble_svc_gap_init();ble_svc_gatt_init();
    chars[0]={};chars[0].uuid=&rx.u;chars[0].access_cb=Access;chars[0].flags=BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_NO_RSP;
    chars[1]={};chars[1].uuid=&tx.u;chars[1].access_cb=Access;chars[1].flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_NOTIFY;chars[1].val_handle=&tx_value;
    chars[2]={};services[0]={};services[0].type=BLE_GATT_SVC_TYPE_PRIMARY;services[0].uuid=&service.u;services[0].characteristics=chars;services[1]={};
    if(ble_gatts_count_cfg(services)||ble_gatts_add_svcs(services)||ble_svc_gap_device_name_set(name_.c_str())){closing_=true;return false;}
    ble_hs_cfg.reset_cb=Reset;ble_hs_cfg.sync_cb=Sync;ble_hs_cfg.gatts_register_cb=nullptr;ble_hs_cfg.store_status_cb=nullptr;
    ble_hs_cfg.sm_bonding=0;ble_hs_cfg.sm_mitm=0;ble_hs_cfg.sm_sc=0;ble_hs_cfg.sm_our_key_dist=0;ble_hs_cfg.sm_their_key_dist=0;
    exited_=false;
    // Own the task handle directly; never use or overwrite the SDK's global
    // nimble_port_freertos task handle retained by the stock driver.
    if(xTaskCreate(Host,"muse_ble",4096,this,5,&task_)!=pdPASS){exited_=true;closing_=true;return false;}
    started_=true;return true;
}
bool DirectBle::Send(const std::string& message){
    if(message.empty()||message.size()>8192||!Current()||!subscribed_)return false;
    std::lock_guard<std::mutex> lock(tx_mutex_);
    const auto connection=connection_.load();if(connection==BLE_HS_CONN_HANDLE_NONE)return false;
    size_t maximum=std::min<size_t>(160,ble_att_mtu(connection)>3?ble_att_mtu(connection)-3:20);
    if(maximum<=3)return false;
    const size_t usable=maximum-3,total=(message.size()+usable-1)/usable;
    if(total>255)return false;
    for(size_t i=0;i<total;++i){
        if(!Current()||!subscribed_||connection_!=connection)return false;
        uint8_t packet[160];packet[0]=0xfe;packet[1]=i;packet[2]=total;size_t count=std::min(usable,message.size()-i*usable);std::memcpy(packet+3,message.data()+i*usable,count);
        auto* value=ble_hs_mbuf_from_flat(packet,count+3);if(!value||ble_gatts_notify_custom(connection,tx_value,value))return false;
        if(i+1<total)vTaskDelay(pdMS_TO_TICKS(50));
    }
    return true;
}
bool DirectBle::StopAndJoin(){
    if(!lease_&&!initialized_&&!task_){clean_=true;return true;}
    closing_=true;if(lease_)CommonBleAuthority().Revoke(lease_);
    if(owner_&&xTaskGetCurrentTaskHandle()!=owner_)return false;
    if(initialized_){
        ble_gap_adv_stop();const auto connection=connection_.load();if(connection!=BLE_HS_CONN_HANDLE_NONE)ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);
        if(started_){if(nimble_port_stop()!=0)return false;while(!exited_)vTaskDelay(pdMS_TO_TICKS(1));if(task_)vTaskDelete(task_);task_=nullptr;started_=false;}
        {std::lock_guard<std::mutex> lock(tx_mutex_);if(nimble_port_deinit()!=ESP_OK)return false;}
        initialized_=false;
    }
    // An init failure has an uncertain controller state: do not release its
    // custody merely because no host task was created.
    else if(lease_&&active.load()==this)return false;
    DirectBle* owned=this;active.compare_exchange_strong(owned,nullptr);connection_=BLE_HS_CONN_HANDLE_NONE;subscribed_=false;receive_={};disconnected_={};current_={};name_.clear();owner_=nullptr;
    const bool released=!lease_||CommonBleAuthority().Release(lease_,true);clean_=released;return released;
}
}
