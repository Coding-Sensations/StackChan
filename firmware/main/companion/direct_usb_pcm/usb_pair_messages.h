#pragma once
#include "usb_pcm_authority.h"
namespace direct_usb_pcm {
// USB-specific grant has no URI or network credential. Parsing cannot issue
// physical consent; actual NumericPairingContext must already be confirmed.
bool ApplyUsbChallenge(const std::string&,companion::NumericPairingContext&,uint32_t epoch,uint16_t stream,uint64_t now);
std::string UsbHello(const companion::NumericPairingContext&);
std::string UsbConfirm(const companion::NumericPairingContext&);
bool ApplyUsbGrant(const std::string&,companion::NumericPairingContext&,const Identity&,uint64_t now);
bool UsbHeartbeat(const std::string&,const companion::NumericPairingContext&,const Identity&,uint64_t now);
bool UsbReady(const std::string&,const companion::NumericPairingContext&,const Identity&,uint64_t now);
std::string UsbStaged(const Identity&);
}
