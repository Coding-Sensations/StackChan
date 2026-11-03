// Portable: c++ -std=c++17 firmware/tests/firmware_install_policy_test.cpp -o /tmp/firmware-install-policy-test
#include "../main/apps/stock_services/firmware_install_policy.h"
#include <cassert>
#include <string>
#include <iostream>

namespace stock_services {
static bool allowed = false;
bool FirmwareInstallAllowed() { return allowed; }
void SetFirmwareInstallAllowed(bool value) { allowed = value; }
}
int main()
{
    using stock_services::IsFirmwareImageUrl;
    const char* accepted[] = {
        "http://47.113.125.164:12800/stackChan/apps", "https://example.com/image.bin",
        "https://cdn.example.com/fw?id=42&signature=abc", "http://localhost:8080/image",
        "https://example.com.:443/image", "https://[::1]/fw", "http://[2001:db8::1]:12800/fw",
        "https://[2001:db8:1:2:3:4:5:6]/fw", "https://[::ffff:192.0.2.1]/fw",
        "https://[1:2:3:4:5:6:192.0.2.1]/fw", "https://example.com:65535/fw",
        "https://example.com/fw%20image.bin", "https://example.com/fw?user=a@b"
    };
    const char* rejected[] = {
        "", "file:///fw", "ftp://example.com/fw", "https:///fw", "http://:80/a",
        "http://user:password@example.com/fw", "http://user@example.com/fw", "http://user%40example.com/fw",
        "https://example.com/#fragment", "https://example.com/white space", "https://example.com/\\fw",
        "http://example.com:", "http://example.com:0/fw", "http://example.com:65536/fw",
        "http://example.com:-1/fw", "http://example.com:abc/fw", "http://example.com:80:90/fw",
        "https://.example.com/fw", "https://example..com/fw", "https://-example.com/fw",
        "https://example-.com/fw", "http://256.0.0.1/fw", "http://1.2.3/fw", "http://1.2.3.4./fw",
        "https://[::1/fw", "https://[]/fw", "https://[gggg::1]/fw", "https://[1:2:3]/fw",
        "https://[1:2:3:4:5:6:7:8:9]/fw", "https://[1::2::3]/fw", "https://[1:2:3:4:5:6:7:8::]/fw",
        "https://[::1]suffix/fw", "https://[::1]:/fw", "https://[::1]:65536/fw",
        "https://::1/fw", "https://[fe80::1%25eth0]/fw", "https://[::ffff:256.0.0.1]/fw"
    };
    for (auto url : accepted) { if (!IsFirmwareImageUrl(url)) std::cerr << "Rejected valid URL: " << url << '\n'; assert(IsFirmwareImageUrl(url)); }
    for (auto url : rejected) { if (IsFirmwareImageUrl(url)) std::cerr << "Accepted invalid URL: " << url << '\n'; assert(!IsFirmwareImageUrl(url)); }
    const std::string prefix = "https://example.com/";
    assert(IsFirmwareImageUrl(prefix + std::string(512 - prefix.size(), 'a')));
    assert(!IsFirmwareImageUrl(prefix + std::string(513 - prefix.size(), 'a')));
    assert(!IsFirmwareImageUrl("https://" + std::string(64, 'a') + ".com/fw"));
    assert(!IsFirmwareImageUrl(std::string("https://example.com/fw") + '\0' + "suffix"));
    assert(!IsFirmwareImageUrl(std::string("https://example.com/fw") + '\n'));
    assert(!stock_services::FirmwareInstallAllowed());
    { stock_services::FirmwareInstallScope scope; assert(stock_services::FirmwareInstallAllowed()); }
    assert(!stock_services::FirmwareInstallAllowed());
    try { stock_services::FirmwareInstallScope scope; throw 1; } catch (...) {}
    assert(!stock_services::FirmwareInstallAllowed());
    std::cout << "firmware install policy tests passed\n";
}
