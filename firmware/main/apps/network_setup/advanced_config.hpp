#pragma once
// Pure validation shared by browser submissions and portable host tests.
#include "../stock_services/firmware_install_policy.h"
#include <cmath>
#include <string>
namespace network_setup {
inline bool ValidTxPower(double value) {
    return value >= 8 && value <= 84 && std::floor(value) == value;
}
inline bool ValidActivationUrl(const std::string& url) {
    // Empty restores the original default activation endpoint. Validation alone
    // never grants firmware installation permission or performs networking.
    return url.empty() || (url.size() <= 255 && stock_services::IsFirmwareImageUrl(url));
}
}
