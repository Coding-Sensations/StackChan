// Portable host tests; no ESP-IDF, networking, or NVS access.
#include "../main/apps/network_setup/advanced_config.hpp"
#include <cassert>
#include <limits>
#include <string>
int main() {
    using network_setup::ValidActivationUrl;
    using network_setup::ValidTxPower;
    for (const char* url : {"", "https://api.example.com/activation", "http://localhost:8080/api?lang=en", "http://192.168.1.2/", "https://[::1]:443/api", "https://example.com./api", "https://example.com/a%20b?key=value"})
        assert(ValidActivationUrl(url));
    for (const char* url : {"ftp://example.com", "https://", "http://?q=1", "https://user:secret@example.com/api", "https://example.com/#fragment", "https://example.com/a b", "https://example.com/\n", "http://example.com:0/", "http://example.com:65536/", "http://example.com:123456/", "http://example.com:abc/", "http://example.com:/", "http://example.com:80:90/", "http://[::1", "http://[]/", "http://[abc]/", "http://[1:2:3]/", "http://[1::2::3]/", "http://[12345::]/", "http://[1:2:3:4:5:6:7:8:9]/", "http://999.1.1.1/", "http://example%40evil.com/", "http://[::1]junk/", "http://./", "http://-host/", "http://host-/", "http://a..b/", "http://example.com\\evil/"})
        assert(!ValidActivationUrl(url));
    assert(ValidActivationUrl("https://example.com:" + std::string("65535") + "/"));
    std::string boundary = "https://example.com/";
    boundary.append(255 - boundary.size(), 'a');
    assert(ValidActivationUrl(boundary));
    boundary.push_back('a');
    assert(!ValidActivationUrl(boundary));
    std::string nul = "https://example.com/";
    nul.push_back('\0'); nul += "hidden";
    assert(!ValidActivationUrl(nul));
    assert(!ValidActivationUrl("https://example.com/" + std::string(1, char(0x80))));
    for (double power : {8., 20., 28., 34., 44., 52., 56., 60., 66., 72., 80., 84.}) assert(ValidTxPower(power));
    for (double power : {-1., 0., 7., 7.99, 8.5, 84.01, 85., 255.}) assert(!ValidTxPower(power));
    assert(!ValidTxPower(std::numeric_limits<double>::infinity()));
    assert(!ValidTxPower(-std::numeric_limits<double>::infinity()));
    assert(!ValidTxPower(std::numeric_limits<double>::quiet_NaN()));
}
