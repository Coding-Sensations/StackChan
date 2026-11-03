#pragma once
#include <cstdint>
namespace usb_composite_v2 {
struct BootQualification {
 bool explicit_boot_selection=false,physical_qualified=false,power_qualified=false;
 bool legacy_owner_closed=false,self_powered=true;
 int vbus_gpio=-1;unsigned bus_power_ma=0;
 uint16_t vid=0,pid=0;const char* serial=nullptr;
};
enum class BootEntryResult { NotSelected, QualificationRequired, StartRefused, Started };
// Called once on CPU0 before any console/provisioning owner is created.
// The SDK backend still verifies actual legacy quiescence and PHY custody.
// Default construction cannot initialize a PHY or change persistent selection.
template<class Start> BootEntryResult EnterQualifiedComposite(const BootQualification& q,Start&& start){
 if(!q.explicit_boot_selection)return BootEntryResult::NotSelected;
 if(!q.physical_qualified||!q.power_qualified||!q.legacy_owner_closed||
    !q.vid||!q.pid||q.vid==0xffff||q.pid==0xffff||(q.vid==0x303a&&q.pid==0x1001)||
    !q.serial||!q.serial[0]||q.bus_power_ma>500||
    (q.self_powered?q.vbus_gpio<0:q.bus_power_ma==0))return BootEntryResult::QualificationRequired;
 return start(q)?BootEntryResult::Started:BootEntryResult::StartRefused;
}
}
