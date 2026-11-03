#include "startup.h"
#include "cdc_session.hpp"
#include <optional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace usb_composite_v2 {
Integration BindProductCallbacks(Integration); // Product callbacks only; no hardware initialization.
namespace {bool requested=false;std::optional<TinyUsbBridge> bridge{std::in_place,BindProductCallbacks(Integration{})};std::optional<BootMode<TinyUsbBridge>> mode;}
bool StartCompositeAtBoot(Integration binding,uint16_t vid,uint16_t pid,bool physical_qualification){
 if(!physical_qualification||requested)return false;
 requested=true;bridge.emplace(BindProductCallbacks(binding));mode.emplace(*bridge);
 return mode->Start(true,vid,pid);
}
BootEntryResult StartQualifiedCompositeAtBoot(const BootQualification& q){
 if(q.explicit_boot_selection&&xPortGetCoreID()!=0)return BootEntryResult::StartRefused;
 return EnterQualifiedComposite(q,[](const BootQualification& qualified){
  Integration binding;binding.legacy_owner_closed=qualified.legacy_owner_closed;
  binding.power_qualified=qualified.power_qualified;binding.self_powered=qualified.self_powered;
  binding.vbus_gpio=qualified.vbus_gpio;binding.bus_power_ma=qualified.bus_power_ma;binding.serial=qualified.serial;
  return StartCompositeAtBoot(binding,qualified.vid,qualified.pid,qualified.physical_qualified);
 });
}
bool CompositeRequested(){return requested;}
TinyUsbBridge* CompositeBridge(){return mode&&mode->Current()==Mode::Composite?&*bridge:nullptr;}
bool CloseComposite(){return cdc_session_owner.load()==nullptr&&(!mode||mode->Close());} // Failure retains objects/claim; never fall back to live Serial/JTAG.
}
