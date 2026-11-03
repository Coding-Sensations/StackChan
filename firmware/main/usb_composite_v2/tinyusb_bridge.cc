#include "tinyusb_bridge.h"
#include "descriptors.hpp"
#include <sdkconfig.h>
#include <cstring>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
#include <tinyusb.h>
#include <tinyusb_default_config.h>
#if CFG_TUD_CDC != 1 || CFG_TUD_HID != 1 || CFG_TUD_MIDI != 1
#error "Composite v2 requires exactly one CDC, HID and MIDI class"
#endif
namespace {
usb_composite_v2::TinyUsbBridge* active=nullptr;
tusb_desc_device_t device{};
std::array<uint8_t,sizeof(usb_composite_v2::configuration)> config_descriptor{};
const char language[]={0x09,0x04};
const char* strings[]={language,"StackChan","StackChan Composite",nullptr,"Companion CDC","Keyboard","MIDI"};
void event_cb(tinyusb_event_t* e,void* p){auto bridge=static_cast<usb_composite_v2::TinyUsbBridge*>(p);if(e->id==TINYUSB_EVENT_ATTACHED)bridge->Attached();else if(e->id==TINYUSB_EVENT_DETACHED)bridge->Detached();}
}
#endif
namespace usb_composite_v2 {
Connection TinyUsbBridge::State()const{
 std::lock_guard<std::mutex> control(control_mutex_);std::lock_guard<std::mutex> report(mutex_);
 if(!operational_)return Connection::Stopped;
 if(reports_.Suspended())return Connection::Suspended;
 if(!reports_.Mounted())return Connection::Detached;
 return reports_.CanOperate()?Connection::Ready:Connection::Settling;
}
bool TinyUsbBridge::Available()const{
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
 return binding_.power_qualified&&binding_.bus_power_ma<=500&&(binding_.self_powered||binding_.bus_power_ma>0)&&(!binding_.self_powered||binding_.vbus_gpio>=0)&&binding_.serial&&binding_.serial[0];
#else
 return false;
#endif
}
bool TinyUsbBridge::Install(uint16_t vid,uint16_t pid){
 std::lock_guard<std::mutex> control(control_mutex_);
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
 if(!Available()||active||installed_)return false;
 auto raw=Device(vid,pid);static_assert(sizeof(device)==18);std::memcpy(&device,raw.data(),raw.size());strings[3]=binding_.serial;
 auto cfg=TINYUSB_DEFAULT_CONFIG(event_cb,this);cfg.phy.skip_setup=true;cfg.phy.self_powered=binding_.self_powered;cfg.phy.vbus_monitor_io=binding_.vbus_gpio;
 cfg.descriptor.device=&device;cfg.descriptor.string=strings;cfg.descriptor.string_count=7;config_descriptor=Configuration(binding_.self_powered,binding_.bus_power_ma);cfg.descriptor.full_speed_config=config_descriptor.data();
 active=this;installed_=true;operational_=false;operational_=tinyusb_driver_install(&cfg)==ESP_OK;return operational_; // Failed partial install retains custody.
#else
 (void)vid;(void)pid;return false;
#endif
}
bool TinyUsbBridge::StopJoined(){
 std::lock_guard<std::mutex> control(control_mutex_);
 operational_=false;
 if(!installed_)return true;
 #if CONFIG_STACKCHAN_USB_COMPOSITE_V2
 if(tinyusb_driver_uninstall()!=ESP_OK)return false;
#else
 return false;
#endif
 installed_=false;
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
 if(active==this)active=nullptr;
#endif
 Detached();return true;
}
bool TinyUsbBridge::Hid(const std::array<uint8_t,8>& r){
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
 return operational_&&tud_hid_ready()&&tud_hid_report(0,r.data(),r.size());
#else
 (void)r;return false;
#endif
}
bool TinyUsbBridge::Midi(const std::array<uint8_t,4>& p){
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
 return operational_&&tud_midi_packet_write(p.data());
#else
 (void)p;return false;
#endif
}
void TinyUsbBridge::Attached(){++cdc_epoch_;std::lock_guard<std::mutex> l(mutex_);reports_.Mounted(*this);}
void TinyUsbBridge::Detached(){++cdc_epoch_;{std::lock_guard<std::mutex> tx(cdc_tx_mutex_);cdc_issued_=cdc_completed_=0;cdc_tx_failed_=false;}std::lock_guard<std::mutex> l(mutex_);reports_.Lost(*this,false);}
void TinyUsbBridge::Suspend(){++cdc_epoch_;std::lock_guard<std::mutex> l(mutex_);reports_.Lost(*this,true);}
void TinyUsbBridge::Resume(){++cdc_epoch_;std::lock_guard<std::mutex> l(mutex_);reports_.Resume(*this);}
void TinyUsbBridge::HidComplete(){std::lock_guard<std::mutex> l(mutex_);reports_.HidComplete();}
bool TinyUsbBridge::ArmKeyboard(bool physical){std::lock_guard<std::mutex> control(control_mutex_);std::lock_guard<std::mutex> l(mutex_);return reports_.ArmKeyboard(physical);}
bool TinyUsbBridge::Keys(uint8_t mods,const std::array<uint8_t,6>& keys){std::lock_guard<std::mutex> control(control_mutex_);std::lock_guard<std::mutex> l(mutex_);return reports_.Press(mods,keys)&&reports_.SendKeys(*this);}
void TinyUsbBridge::Release(){std::lock_guard<std::mutex> control(control_mutex_);std::lock_guard<std::mutex> l(mutex_);reports_.Release();reports_.AllNotesOff();LocalSilence();}
size_t TinyUsbBridge::GetReport(bool input,uint8_t* p,size_t n){
 std::lock_guard<std::mutex> l(mutex_);if(!p)return 0;
 if(input){if(n<8)return 0;auto report=reports_.InputReport();std::memcpy(p,report.data(),report.size());return report.size();}
 if(!n)return 0;
 p[0]=reports_.leds;return 1;
}
bool TinyUsbBridge::SendNote(uint8_t channel,uint8_t note,uint8_t velocity){std::lock_guard<std::mutex> control(control_mutex_);std::lock_guard<std::mutex> l(mutex_);return reports_.MidiOutput(*this,channel,note,velocity);}
bool TinyUsbBridge::SetLeds(const uint8_t* p,size_t n){std::lock_guard<std::mutex> l(mutex_);return reports_.SetLeds(p,n);}
void TinyUsbBridge::Service(){
 std::lock_guard<std::mutex> control(control_mutex_);if(!operational_)return;
 std::lock_guard<std::mutex> l(mutex_);reports_.Flush(*this);
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
 std::array<uint8_t,4> p{};for(unsigned i=0;i<16&&tud_midi_packet_read(p.data());++i)reports_.MidiInput(*this,p);
#endif
}
int TinyUsbBridge::CdcRead(uint8_t* p,size_t n){
 std::lock_guard<std::mutex> control(control_mutex_);
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
 if(!operational_||!tud_cdc_connected()||!p||!n)return 0;
 return int(tud_cdc_read(p,std::min<size_t>(n,128)));
#else
 (void)p;(void)n;return -1;
#endif
}
int TinyUsbBridge::CdcWrite(const uint8_t* p,size_t n){
 std::lock_guard<std::mutex> control(control_mutex_);
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
 std::lock_guard<std::mutex> tx(cdc_tx_mutex_);
 if(!operational_||!tud_cdc_connected()||!p||!n||n>256||cdc_tx_failed_||cdc_issued_!=cdc_completed_||cdc_issued_>UINT64_MAX-n||tud_cdc_write_available()!=CFG_TUD_CDC_TX_BUFSIZE)return 0;
 auto sent=tud_cdc_write(p,n);cdc_issued_+=sent;tud_cdc_write_flush();return int(sent);
#else
 (void)p;(void)n;return -1;
#endif
}
bool TinyUsbBridge::OnOwnerCore()const{return xPortGetCoreID()==0;}
bool TinyUsbBridge::CdcAlive()const{
 std::lock_guard<std::mutex> control(control_mutex_);
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
 return operational_&&tud_ready()&&tud_cdc_connected();
#else
 return false;
#endif
}
void TinyUsbBridge::CdcLineState(bool dtr){if(cdc_dtr_.exchange(dtr)!=dtr)++cdc_epoch_;} // RTS/baud never reset or authorize.
bool TinyUsbBridge::CdcTxIdle()const{std::lock_guard<std::mutex> tx(cdc_tx_mutex_);return !cdc_tx_failed_&&cdc_issued_==cdc_completed_;}
void TinyUsbBridge::CdcTxProgress(unsigned bytes,bool success){
 std::lock_guard<std::mutex> tx(cdc_tx_mutex_);
 if(!success||bytes>cdc_issued_-cdc_completed_)cdc_tx_failed_=true;
 else cdc_completed_+=bytes;
}
bool TinyUsbBridge::CdcFlushAck(uint64_t epoch,unsigned milliseconds){
 auto began=esp_timer_get_time();
 do{
  if(epoch!=CdcEpoch()||!CdcAlive())return false;
  if(CdcTxIdle())return true;
  vTaskDelay(pdMS_TO_TICKS(1));
 }while(esp_timer_get_time()-began<static_cast<int64_t>(milliseconds)*1000);
 return false; // No retry or claim of a receipt still queued/in flight.
}

}
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
extern "C" uint8_t const* tud_hid_descriptor_report_cb(uint8_t){return usb_composite_v2::keyboard_report.data();}
extern "C" uint16_t tud_hid_get_report_cb(uint8_t itf,uint8_t id,hid_report_type_t type,uint8_t* data,uint16_t length){if(!active||itf||id||(type!=HID_REPORT_TYPE_INPUT&&type!=HID_REPORT_TYPE_OUTPUT))return 0;return active->GetReport(type==HID_REPORT_TYPE_INPUT,data,length);}
extern "C" void tud_hid_set_report_cb(uint8_t itf,uint8_t id,hid_report_type_t type,uint8_t const* data,uint16_t length){if(active&&itf==0&&id==0&&type==HID_REPORT_TYPE_OUTPUT)active->SetLeds(data,length);}
extern "C" void tud_hid_report_complete_cb(uint8_t,uint8_t const*,uint16_t){if(active)active->HidComplete();}
#if !CONFIG_TINYUSB_SUSPEND_CALLBACK && !CONFIG_TINYUSB_RESUME_CALLBACK
extern "C" void tud_suspend_cb(bool){if(active)active->Suspend();}
extern "C" void tud_resume_cb(){if(active)active->Resume();}
#else
#error "Use direct TinyUSB suspend/resume callbacks for this reviewed adapter"
#endif
#endif

#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
extern "C" void stackchan_cdc_line_state(uint8_t itf,bool dtr,bool){if(active&&itf==0)active->CdcLineState(dtr);}
extern "C" void stackchan_cdc_tx_progress(uint8_t itf,unsigned bytes,bool success){if(active&&itf==0)active->CdcTxProgress(bytes,success);}
#endif

namespace usb_composite_v2 {
bool TinyUsbBridge::ArmLocalMidi(bool physical){
 std::lock_guard<std::mutex> control(control_mutex_);std::lock_guard<std::mutex> report(mutex_);
#if CONFIG_STACKCHAN_USB_COMPOSITE_V2
 if(physical&&(!operational_||!tud_ready()))return false;
#else
 if(physical)return false;
#endif
 return binding_.local_arm&&binding_.local_arm(physical);
}
}

namespace usb_composite_v2 { void TinyUsbBridge::ReleaseKeyboard(){std::lock_guard<std::mutex> control(control_mutex_);std::lock_guard<std::mutex> report(mutex_);reports_.Release();} }
