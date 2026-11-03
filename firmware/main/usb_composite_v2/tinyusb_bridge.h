#pragma once
#include "controller.hpp"
#include "sdk_phy.h"
#include <mutex>
#include <atomic>
namespace usb_composite_v2 {
enum class Connection { Stopped, Detached, Suspended, Settling, Ready };
struct Integration {
 bool legacy_owner_closed=false;
 // Factory battery operation requires qualified VBUS sensing; do not invent GPIO.
 bool power_qualified=false,self_powered=true;int vbus_gpio=-1;unsigned bus_power_ma=0;
 const char* serial=nullptr;
 // Concrete cooperative esp_tinyusb 2.1.0 teardown is supplied by this candidate.
 // Called under report lock: must be nonblocking, non-reentrant, never call bridge methods.
 bool (*local_note)(uint8_t,uint8_t,uint8_t)=nullptr;
 void (*local_silence)()=nullptr;
 bool (*local_arm)(bool)=nullptr;
};
class TinyUsbBridge {
 SdkPhy phy_;Integration binding_;Reports reports_;mutable std::mutex mutex_;mutable std::mutex control_mutex_;bool installed_=false,operational_=false;std::atomic<uint64_t> cdc_epoch_{1};mutable std::mutex cdc_tx_mutex_;uint64_t cdc_issued_=0,cdc_completed_=0;bool cdc_tx_failed_=false;std::atomic<bool> cdc_dtr_{false};
public:
 explicit TinyUsbBridge(Integration binding):binding_(binding){}
 bool Available()const;
 Connection State()const;
 bool LegacyQuiescent()const{return phy_.LegacyQuiescent(binding_.legacy_owner_closed);}
 bool OpenPhy(){return phy_.Open(binding_.self_powered,binding_.vbus_gpio);}bool DeletePhy(){return phy_.Delete();}
 bool Install(uint16_t vid,uint16_t pid);bool StopJoined();
private:
 friend class Reports;
 bool Hid(const std::array<uint8_t,8>& report);bool Midi(const std::array<uint8_t,4>& packet);
 void LocalSilence(){if(binding_.local_silence)binding_.local_silence();}
 bool LocalNote(uint8_t channel,uint8_t note,uint8_t velocity){return binding_.local_note&&binding_.local_note(channel,note,velocity);}
public:
 // Control entry points serialize driver operations with joined shutdown.
 // Callbacks below use only the report mutex, never control_mutex_.
 void Attached();void Detached();void Suspend();void Resume();void HidComplete();
 bool ArmKeyboard(bool physical);bool ArmLocalMidi(bool physical);bool Keys(uint8_t mods,const std::array<uint8_t,6>& keys);void Release();void ReleaseKeyboard();
 bool SetLeds(const uint8_t* data,size_t size);
 size_t GetReport(bool input,uint8_t* data,size_t bytes);
 bool SendNote(uint8_t channel,uint8_t note,uint8_t velocity);
 // Nonblocking service: one full-speed MIDI endpoint packet (16 events) per call.
 bool OnOwnerCore()const;bool CdcAlive()const;
 uint64_t CdcEpoch()const{return cdc_epoch_.load();}
 bool CdcTxIdle()const;
 bool CdcFlushAck(uint64_t epoch,unsigned milliseconds);
 void CdcLineState(bool dtr);void CdcTxProgress(unsigned bytes,bool success);
 void Service();int CdcRead(uint8_t* data,size_t bytes);int CdcWrite(const uint8_t* data,size_t bytes);
};
}
