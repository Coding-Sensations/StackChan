#pragma once
#include <string>
namespace stock_services {
bool OfferFirmwareInstall(const std::string& url,const std::string& version={});
bool PrepareOriginalFirmwareWifi();
// Only after the installer has joined; owns its retained result panel.
void ResetFirmwareInstallUi();
}
