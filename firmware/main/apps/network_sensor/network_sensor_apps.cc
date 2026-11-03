#include "network_sensor_apps.h"
#include "ltr553_proximity.hpp"
#include "ltr553_idf_driver.hpp"
#include "wifi_wol_idf.hpp"
#include <companion/boot.h>
#include <apps/stock_shell/state_storage_binding.hpp>
#include <apps/stock_shell/ir_storage_binding.hpp>
#include <hal/hal.h>
#include <assets/assets.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <lvgl.h>
#include <apps/common/tool_visual.hpp>
#include <atomic>
#include <cstdio>
#include <vector>
#include <array>
#include <string>
namespace stock_tools {
namespace {
using stack_tools::Ltr553IdfDriver;
using stack_tools::Ltr553Proximity;
using stack_tools::ProximityError;
using stack_tools::ProximityState;
using stack_tools::ObserveExistingWifi;
using stack_tools::MacAddress;
using stack_tools::ParseMac;
using stack_tools::ParseIpv4;
using stack_tools::IdfWakeTransport;
using stack_tools::SendWakeOnce;
using stack_tools::WakeResult;
uint64_t Now(){return static_cast<uint64_t>(esp_timer_get_time()/1000);}
class ToolPage : public mooncake::AppAbility {
    struct Callback { ToolPage* page;int action; };
    std::vector<std::unique_ptr<Callback>> callbacks_;
protected:
    lv_obj_t* root_=nullptr;
    std::atomic<int> action_{0};
    void Clear(){if(root_)lv_obj_clean(root_);callbacks_.clear();action_=0;}
    lv_obj_t* Text(const char* value,int x,int y,int w,int h){
        auto label=lv_label_create(root_);lv_label_set_text(label,value);lv_obj_set_pos(label,x,y);lv_obj_set_size(label,w,h);
        lv_obj_set_style_text_font(label,&lv_font_montserrat_16,0);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);return label;
    }
    lv_obj_t* Button(const char* title,int x,int y,int w,int h,int action){
        auto button=stock_ui::Button(root_,title,x,y,w,h<40?40:h);
        auto callback=std::make_unique<Callback>(Callback{this,action});
        lv_obj_add_event_cb(button,[](lv_event_t* event){auto* c=static_cast<Callback*>(lv_event_get_user_data(event));c->page->action_=c->action;},LV_EVENT_CLICKED,callback.get());
        callbacks_.push_back(std::move(callback));return button;
    }
    void Header(const char* title){Clear();Button("Back",8,4,84,44,1);Text(title,104,12,208,32);}
    void OpenRoot(){
        root_=lv_obj_create(lv_screen_active());lv_obj_set_pos(root_,0,0);lv_obj_set_size(root_,320,240);
        lv_obj_set_style_pad_all(root_,0,0);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_text_font(root_,&lv_font_montserrat_16,0);
        stock_ui::Page(root_);
        lv_obj_set_style_text_color(root_,lv_color_hex(stock_ui::Ink),0);
    }
    void Icon(const char* name){setAppInfo().name=name;static auto icon=assets::get_image("icon_controller.bin");setAppInfo().icon=&icon;}
public:
    void onClose() override{LvglLockGuard lock;if(root_)lv_obj_delete(root_);root_=nullptr;callbacks_.clear();action_=0;}
};
// Boot lifetime owners preserve failed cleanup across Shell uninstall/destruction.
Ltr553Proximity<Ltr553IdfDriver>& BootProximitySensor(){
    static Ltr553IdfDriver driver;
    static Ltr553Proximity<Ltr553IdfDriver> sensor(driver,300,600);
    return sensor;
}
IdfWakeTransport& BootWakeTransport(){static IdfWakeTransport transport;return transport;}
class ProximityApp final : public ToolPage {
    Ltr553Proximity<Ltr553IdfDriver>& sensor_=BootProximitySensor();
    bool running_=false,stop_pending_=false;
    uint64_t next_poll_=0;
    lv_obj_t* info_=nullptr;
    const char* ErrorText() const {
        switch(sensor_.Error()){
            case ProximityError::None:return "Ready";
            case ProximityError::BusUnavailable:return "Sensor unavailable";
            case ProximityError::ReadFailed:return "Sensor read failed";
            case ProximityError::WrongDevice:return "Sensor identity mismatch";
            case ProximityError::ForeignActive:return "Another app owns sensor";
            case ProximityError::ConfigureFailed:return "Sensor setup failed";
            case ProximityError::RestoreFailed:return "Sensor restore pending";
        }
        return "Sensor unavailable";
    }
    void Render(){
        Header("Proximity");
        info_=Text("Stopped. Tap Start.",8,60,304,82);
        Text("Raw counts; no calibrated distance\nNear 600 / far 300",8,144,304,32);
        Button(stop_pending_?"Retry Stop":running_?"Stop":"Start",8,184,304,48,2);
    }
    void Refresh(){
        char text[180];auto sample=sensor_.FreshTelemetry(Now(),500);
        const char* state=sample.state==ProximityState::Near?"Near":sample.state==ProximityState::Far?"Far":"Unknown";
        if(stop_pending_)std::snprintf(text,sizeof(text),"Sensor restore pending.\nRetry Stop.");
        else if(!running_)std::snprintf(text,sizeof(text),"Stopped.\n%s",ErrorText());
        else if(!sample.valid)std::snprintf(text,sizeof(text),"%s\n%s\n%s",state,sample.saturated?"Saturated": "No fresh measurement",ErrorText());
        else std::snprintf(text,sizeof(text),"%s\nRaw %u / filtered %u\nAge %llu ms",state,sample.raw,sample.filtered,static_cast<unsigned long long>(Now()-sample.observed_ms));
        lv_label_set_text(info_,text);
        const auto color=sample.valid?(sample.state==ProximityState::Near?0x164831:0x103449):0x10232d;
        lv_obj_set_style_bg_color(root_,lv_color_hex(color),0);
    }
    bool Stop(){const bool closed=sensor_.Close();stop_pending_=!closed;running_=false;return closed;}
public:
    ProximityApp(){Icon("PROXIMITY");}
    void onOpen() override{LvglLockGuard lock;OpenRoot();running_=false;stop_pending_=sensor_.HasCustody() && !running_;Render();Refresh();}
    void onRunning() override{
        const int action=action_.exchange(0);
        if(action==1){if(Stop()){close();return;}LvglLockGuard lock;Render();Refresh();return;}
        if(action==2){
            if(running_||stop_pending_)Stop();
            else {running_=sensor_.Start(Now());stop_pending_=sensor_.HasCustody() && !running_;next_poll_=Now();}
            LvglLockGuard lock;Render();Refresh();
        }
        if(running_ && Now()>=next_poll_){next_poll_=Now()+100;sensor_.Poll(Now());LvglLockGuard lock;Refresh();}
    }
    void onClose() override{Stop();ToolPage::onClose();info_=nullptr;}
};
class NetworkToolsApp final : public ToolPage {
    std::string name_,mac_,broadcast_,notice_="Target not saved on SD",draft_;
    PersistentState<SavedWakeTargets> targets_{*WakeFiles()};size_t selected_=0;std::string selected_name_;
    void Pick(){const auto& records=targets_.Library().Records();if(records.empty()){selected_name_.clear();name_.clear();mac_.clear();broadcast_.clear();return;}selected_%=records.size();const auto& target=records[selected_];selected_name_=name_=target.name;mac_=target.mac;broadcast_=target.broadcast;}
    void Load(){if(targets_.Load()){selected_=0;Pick();}notice_=IrStoreMessage(targets_.Status());}
    int editing_=0,key_page_=0;
    bool wifi_page_=true,survey_page_=false,coverage_page_=false,survey_back_=false,survey_details_=false,wake_manage_=false;
    companion::survey::Snapshot snapshot_{};
    companion::survey::Samples samples_;std::string location_;size_t survey_selected_=0;
    const char* Security(uint8_t auth){switch(wifi_auth_mode_t(auth)){case WIFI_AUTH_OPEN:return "Open";case WIFI_AUTH_WEP:return "WEP";case WIFI_AUTH_WPA_PSK:return "WPA";case WIFI_AUTH_WPA2_PSK:return "WPA2";case WIFI_AUTH_WPA_WPA2_PSK:return "WPA/WPA2";case WIFI_AUTH_WPA2_ENTERPRISE:return "WPA2 enterprise";case WIFI_AUTH_WPA3_PSK:return "WPA3";case WIFI_AUTH_WPA2_WPA3_PSK:return "WPA2/WPA3";case WIFI_AUTH_OWE:return "OWE";default:return "Other/unknown";}}
    void SurveyRender(){
        Header(coverage_page_?"Signal trend":survey_details_?"AP details":"Wi-Fi survey");input_=status_=nullptr;
        companion::CopyWifiSurveySnapshot(snapshot_);const auto& snapshot=snapshot_;char text[512];
        if(coverage_page_){
            const companion::survey::Sample* points[5]{};unsigned count=0;
            if(snapshot.count){const auto& bssid=snapshot.aps[survey_selected_%snapshot.count].bssid;for(size_t i=samples_.Count();i>0&&count<5;--i)if(samples_.At(i-1).bssid==bssid)points[count++]=&samples_.At(i-1);}
            Text("Manual samples in order; RAM only",8,52,304,24);
            if(count){auto chart=lv_chart_create(root_);lv_obj_set_pos(chart,8,80);lv_obj_set_size(chart,304,82);lv_chart_set_type(chart,LV_CHART_TYPE_LINE);lv_chart_set_axis_range(chart,LV_CHART_AXIS_PRIMARY_Y,-128,0);lv_chart_set_point_count(chart,count);auto series=lv_chart_add_series(chart,lv_color_hex(stock_ui::Accent),LV_CHART_AXIS_PRIMARY_Y);for(unsigned i=0;i<count;++i)lv_chart_set_series_value_by_id(chart,series,i,points[count-i-1]->rssi);std::snprintf(text,sizeof(text),"Latest: %s %d dBm",points[0]->location.data(),points[0]->rssi);Text(text,8,166,304,24);}
            else Text("No samples for this AP yet",8,100,304,48);
            Button("AP overview",8,194,304,40,21);return;
        }
        if(survey_details_){
            if(snapshot.count){const auto& ap=snapshot.aps[survey_selected_%snapshot.count];std::snprintf(text,sizeof(text),"%02X:%02X:%02X:%02X:%02X:%02X\nChannel %u | %s\n%d dBm | observed %llus ago\n%zu APs%s | %zu RAM samples\n%s",ap.bssid[0],ap.bssid[1],ap.bssid[2],ap.bssid[3],ap.bssid[4],ap.bssid[5],ap.channel,Security(ap.security),ap.rssi,static_cast<unsigned long long>((Now()-snapshot.observed_ms)/1000),snapshot.count,snapshot.truncated?"+":"",samples_.Count(),companion::survey::Label(snapshot.phase));Text(text,8,58,304,128);}
            else Text(companion::survey::Label(snapshot.phase),8,58,304,128);
            Button("AP overview",8,194,304,40,22);return;
        }
        if(snapshot.count){const auto& ap=snapshot.aps[survey_selected_%snapshot.count];auto title=Text(ap.ssid[0]?ap.ssid.data():"<hidden SSID>",8,50,214,24);lv_label_set_long_mode(title,LV_LABEL_LONG_SCROLL_CIRCULAR);std::snprintf(text,sizeof(text),"%d dBm",ap.rssi);Text(text,230,50,82,24);auto meter=lv_bar_create(root_);status_=Text(notice_.c_str(),8,74,304,18);lv_label_set_long_mode(status_,LV_LABEL_LONG_SCROLL_CIRCULAR);lv_obj_set_pos(meter,8,94);lv_obj_set_size(meter,304,4);lv_bar_set_range(meter,-128,0);lv_bar_set_value(meter,ap.rssi,LV_ANIM_OFF);}
        else status_=Text(companion::survey::Label(snapshot.phase),8,50,304,42);
        Button("Next AP",8,100,148,40,16);Button("Scan",164,100,148,40,17);
        Button("Sample",8,146,148,40,20);Button(location_.empty()?"Name location":location_.c_str(),164,146,148,40,19);
        Button("Coverage",8,192,148,40,21);Button("Details",164,192,148,40,22);
    }
    lv_obj_t* input_=nullptr;
    lv_obj_t* status_=nullptr;
    uint64_t refresh_at_=0;
    const std::array<const char*,3> mac_keys_{{"0123456789","abcdefABCD","EF:"}};
    const std::array<const char*,4> name_keys_{{"abcdefghij","klmnopqrst","uvwxyz0123","456789 _-."}};
    const std::array<const char*,2> ip_keys_{{"0123456789","."}};
    void Render(){
        if(survey_page_&&!editing_){SurveyRender();return;}
        if(wifi_page_&&!editing_){
            Header("Network tools");input_=nullptr;stock_ui::Card(root_,8,52,304,42);status_=Text("",16,56,288,38);
            Button("Nearby AP survey",8,100,304,40,15);
            Button("Connect saved Wi-Fi",8,146,304,40,13);
            Button("Wake targets",8,192,304,40,14);return;
        }
        Header(editing_==1?"MAC address":editing_==2?"IPv4 broadcast":editing_==3?"Target name":editing_==4?"Location name":"Wake targets");
        input_=status_=nullptr;
        if(editing_){
            input_=lv_textarea_create(root_);lv_obj_set_pos(input_,8,52);lv_obj_set_size(input_,304,44);lv_textarea_set_one_line(input_,true);
            lv_textarea_set_max_length(input_,editing_==1?17:editing_==2?15:24);lv_textarea_set_text(input_,draft_.c_str());
            const std::string keys=editing_==1?mac_keys_[key_page_]:editing_==2?ip_keys_[key_page_]:name_keys_[key_page_];
            for(size_t i=0;i<keys.size();++i){char key[]{keys[i],0};Button(key,8+(i%5)*60,100+(i/5)*46,56,44,100+static_cast<unsigned char>(keys[i]));}
            Button("Keys",8,192,88,44,4);Button("<--",100,192,64,44,5);Button("Save",168,192,144,44,6);
        }else{
            if(wake_manage_){
                Button(name_.empty()?"Set target name":name_.c_str(),8,52,220,40,9);Button("Next",232,52,80,40,10);
                Button(mac_.empty()?"Set target MAC":mac_.c_str(),8,98,304,40,2);
                Button(broadcast_.empty()?"Set broadcast":broadcast_.c_str(),8,144,304,40,3);
                Button("Save SD",8,190,96,40,8);Button("Reload",110,190,96,40,11);Button("Delete",212,190,100,40,12);
            }else{
                Text(name_.empty()?"No wake target selected":name_.c_str(),8,52,304,24);stock_ui::Card(root_,8,80,304,64);
                const std::string target=(mac_.empty()?"Set a target MAC":mac_)+"\n"+(broadcast_.empty()?"Set subnet broadcast":broadcast_);Text(target.c_str(),16,84,288,56);
                status_=Text(notice_.c_str(),8,148,304,38);
                Button(BootWakeTransport().PendingClosure()?"Retry close":"Send once",8,192,148,40,7);Button("Targets",164,192,148,40,23);
            }
        }
    }
    void RefreshWifi(){
        if(survey_page_)return;
        if(!status_)return;
        const auto wifi=ObserveExistingWifi();char text[180];
        if(wifi_page_){
            const auto survey=companion::WifiSurveyPhase();
            if(survey==companion::survey::Phase::Scanning||survey==companion::survey::Phase::Stopping||survey==companion::survey::Phase::CleanupFailed)std::snprintf(text,sizeof(text),"%s\nOpen AP survey for cleanup",companion::survey::Label(survey));
            else if(wifi.connected&&companion::SavedNetworkPhase()==companion::SavedWifiPhase::RestartRequired)std::snprintf(text,sizeof(text),"Connected: %d dBm\nShared driver: restart to reconnect",wifi.rssi);
            else if(wifi.connected)std::snprintf(text,sizeof(text),"Connected | signal %d dBm",wifi.rssi);
            else if(companion::SavedNetworkPhase()==companion::SavedWifiPhase::Disconnected)std::snprintf(text,sizeof(text),"Wi-Fi disconnected (reason %u)\nConnect saved Wi-Fi to retry",static_cast<unsigned>(companion::SavedNetworkDisconnectReason()));
            else std::snprintf(text,sizeof(text),"%s\nPC metrics unavailable",companion::SavedWifiLabel(companion::SavedNetworkPhase()));
        }else if(wifi.connected)std::snprintf(text,sizeof(text),"WiFi %d dBm | PC unavailable\n%s",wifi.rssi,notice_.c_str());
        else std::snprintf(text,sizeof(text),"%s\n%s",companion::SavedWifiLabel(companion::SavedNetworkPhase()),notice_.c_str());
        lv_label_set_text(status_,text);
    }
public:
    NetworkToolsApp(){Icon("NETWORK TOOLS");}
    void onClose() override{ESP_LOGI("NetworkTools","close");companion::CancelWifiSurvey();ToolPage::onClose();}
    void onOpen() override{ESP_LOGI("NetworkTools","open");Load();LvglLockGuard lock;OpenRoot();editing_=0;wifi_page_=true;if(BootWakeTransport().PendingClosure())notice_="Socket close pending";Render();RefreshWifi();}
    void onRunning() override{
        const int action=action_.exchange(0);
        if(survey_page_){companion::PollWifiSurvey();const auto phase=companion::WifiSurveyPhase();
            if(survey_back_&&(phase==companion::survey::Phase::Idle||phase==companion::survey::Phase::Complete||phase==companion::survey::Phase::Failed)){survey_page_=coverage_page_=survey_back_=false;LvglLockGuard lock;Render();RefreshWifi();return;}
            if(action==1&&!editing_){if(companion::CancelWifiSurvey()){survey_page_=coverage_page_=false;}else survey_back_=true;LvglLockGuard lock;Render();return;}
            if(!editing_){
                if(action==16){++survey_selected_;LvglLockGuard lock;Render();return;}
                if(action==17){companion::CopyWifiSurveySnapshot(snapshot_);const auto prior=snapshot_.generation;const bool started=companion::RequestWifiSurvey();if(started)samples_.BeginScan(location_,prior);notice_=started?"Passive scan requested":"Connection/voice/shared radio busy";LvglLockGuard lock;Render();return;}
                if(action==19){editing_=4;draft_=location_;key_page_=0;LvglLockGuard lock;Render();return;}
                if(action==20){companion::CopyWifiSurveySnapshot(snapshot_);const auto& current=snapshot_;const auto selected=current.count?survey_selected_%current.count:0;notice_=samples_.Record(location_,current,selected)?"Sampled here; RAM only":"Set name + Scan again before Sample";LvglLockGuard lock;Render();return;}
                if(action==21){coverage_page_=!coverage_page_;survey_details_=false;LvglLockGuard lock;Render();return;}
                if(action==22){survey_details_=!survey_details_;coverage_page_=false;LvglLockGuard lock;Render();return;}
                if(Now()>=refresh_at_){refresh_at_=Now()+500;notice_=companion::survey::Label(phase);LvglLockGuard lock;Render();}
            }
        }
        if(!editing_&&action==15&&wifi_page_){survey_page_=true;coverage_page_=survey_details_=false;notice_="Move here, name location, then Scan";LvglLockGuard lock;Render();return;}
        if(action==1){if(editing_){editing_=0;LvglLockGuard lock;Render();RefreshWifi();}else if(wake_manage_){wake_manage_=false;LvglLockGuard lock;Render();RefreshWifi();}else if(!wifi_page_){wifi_page_=true;LvglLockGuard lock;Render();RefreshWifi();}else {ESP_LOGI("NetworkTools","Back to launcher requested");close();}return;}
        if(!editing_&&action==13&&wifi_page_){companion::EnsureSavedNetwork();LvglLockGuard lock;Render();RefreshWifi();return;}
        if(!editing_&&action==23&&!wifi_page_){wake_manage_=true;LvglLockGuard lock;Render();return;}
        if(!editing_&&action==14&&wifi_page_){wifi_page_=false;wake_manage_=false;LvglLockGuard lock;Render();RefreshWifi();return;}
        if(!editing_ && (action==2 || action==3 || action==9)){editing_=action==2?1:action==3?2:3;key_page_=0;draft_=editing_==1?mac_:editing_==2?broadcast_:name_;LvglLockGuard lock;Render();return;}
        if(editing_){
            LvglLockGuard lock;
            if(action>=100 && action<=227)lv_textarea_add_char(input_,action-100);
            if(action==5)lv_textarea_delete_char(input_);
            if(action==4){draft_=lv_textarea_get_text(input_);key_page_=(key_page_+1)%(editing_==1?mac_keys_.size():editing_==2?ip_keys_.size():name_keys_.size());Render();}
            if(action==6){
                const std::string value=lv_textarea_get_text(input_);MacAddress mac{};uint32_t ipv4=0;
                if(editing_==1?ParseMac(value,mac):editing_==2?ParseIpv4(value,ipv4):editing_==4?companion::survey::Samples::ValidLocation(value):NamedIrLibrary::ValidName(value)){
                    (editing_==1?mac_:editing_==2?broadcast_:editing_==4?location_:name_)=value;editing_=0;notice_=survey_page_?"Location named; Scan here before Sample":"Target not saved on SD";Render();RefreshWifi();
                }
            }
            return;
        }
        if(action>=8 && action<=12){
            if(action==8){auto next=targets_.Library();if(next.Save({name_,mac_,broadcast_},name_==selected_name_)&&targets_.Save(next)){selected_name_=name_;notice_="Saved on SD";}else notice_=targets_.Status()==IrStoreStatus::Ready?"Check name/target; name exists":IrStoreMessage(targets_.Status());}
            else if(action==10){if(!targets_.Library().Records().empty()){++selected_;Pick();notice_="Selected saved target";}}
            else if(action==11)Load();
            else if(action==12){auto next=targets_.Library();if(!selected_name_.empty()&&next.Remove(selected_name_)&&targets_.Save(next)){Pick();notice_="Deleted from SD";}else notice_=IrStoreMessage(targets_.Status());}
            LvglLockGuard lock;Render();RefreshWifi();return;
        }
        if(action==7){
            auto& transport=BootWakeTransport();
            if(transport.PendingClosure()){
                notice_=transport.Close()?"Socket closed; no packet sent":"Socket close pending";
            }else{
            const auto result=SendWakeOnce(transport,mac_,broadcast_,ObserveExistingWifi());
            switch(result){
                case WakeResult::Sent:notice_="Sent; wake unverified";break;
                case WakeResult::NoNetwork:notice_="Connect saved Wi-Fi on Network page";break;
                case WakeResult::InvalidTarget:notice_="Check MAC + WiFi broadcast";break;
                case WakeResult::CloseFailed:notice_="Socket closure failed";break;
                default:notice_="Wake packet send failed";break;
            }
            }
            LvglLockGuard lock;Render();
        }
        if(action==7 || Now()>=refresh_at_){refresh_at_=Now()+1000;LvglLockGuard lock;RefreshWifi();}
    }
};
} // namespace
std::unique_ptr<mooncake::AppAbility> MakeProximityApp(){return std::make_unique<ProximityApp>();}
std::unique_ptr<mooncake::AppAbility> MakeNetworkToolsApp(){return std::make_unique<NetworkToolsApp>();}
}
