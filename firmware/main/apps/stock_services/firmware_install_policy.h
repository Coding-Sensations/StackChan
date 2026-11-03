#pragma once
#include <string_view>
#include <cstddef>

namespace stock_services {
// RAM-only permission. Root runtime initializes false; catalog browsing never grants it.
bool FirmwareInstallAllowed();
void SetFirmwareInstallAllowed(bool allowed);

namespace firmware_url_detail {
inline bool IsIpv4(std::string_view host)
{
    unsigned parts = 0;
    while (!host.empty()) {
        auto dot = host.find('.');
        auto part = host.substr(0, dot);
        if (part.empty() || part.size() > 3) return false;
        unsigned value = 0;
        for (char c : part) {
            if (c < '0' || c > '9') return false;
            value = value * 10 + static_cast<unsigned>(c - '0');
        }
        if (value > 255 || ++parts > 4) return false;
        if (dot == std::string_view::npos) return parts == 4;
        host.remove_prefix(dot + 1);
    }
    return false;
}
inline bool IsIpv6(std::string_view host)
{
    // Count 16-bit groups; a single :: must compress at least one group.
    const auto compression = host.find("::");
    if (compression != std::string_view::npos && host.find("::", compression + 2) != std::string_view::npos)
        return false;
    unsigned groups = 0;
    auto count = [&groups](std::string_view side, bool allow_ipv4) {
        while (!side.empty()) {
            auto colon = side.find(':');
            auto group = side.substr(0, colon);
            if (group.empty()) return false;
            if (group.find('.') != std::string_view::npos) {
                if (!allow_ipv4 || colon != std::string_view::npos || !IsIpv4(group)) return false;
                groups += 2;
            } else {
                if (group.size() > 4) return false;
                for (char c : group)
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                        return false;
                ++groups;
            }
            if (colon == std::string_view::npos) return true;
            side.remove_prefix(colon + 1);
            if (side.empty()) return false;
        }
        return true;
    };
    if (compression == std::string_view::npos) return count(host, true) && groups == 8;
    return count(host.substr(0, compression), false) && count(host.substr(compression + 2), true) && groups < 8;
}
inline bool IsHost(std::string_view host)
{
    if (host.empty() || host.size() > 253) return false;
    bool numeric = true;
    for (char c : host) if ((c < '0' || c > '9') && c != '.') numeric = false;
    if (numeric) return IsIpv4(host);
    if (host.back() == '.') host.remove_suffix(1);
    if (host.empty()) return false;
    while (!host.empty()) {
        const auto dot = host.find('.');
        const auto label = host.substr(0, dot);
        if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') return false;
        for (char c : label)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'))
                return false;
        if (dot == std::string_view::npos) return true;
        host.remove_prefix(dot + 1);
    }
    return false;
}
}  // namespace firmware_url_detail

inline bool IsFirmwareImageUrl(std::string_view url)
{
    if (url.empty() || url.size() > 512) return false;
    const std::size_t start = url.rfind("https://", 0) == 0 ? 8 : url.rfind("http://", 0) == 0 ? 7 : 0;
    if (!start) return false;
    for (unsigned char c : url) {
        if (c <= 32 || c >= 127 || c == '\\' || c == '#') return false;
    }
    const auto end = url.find_first_of("/?", start);
    const auto authority = url.substr(start, end == std::string_view::npos ? url.size() - start : end - start);
    if (authority.empty() || authority.find('@') != std::string_view::npos || authority.find('%') != std::string_view::npos)
        return false;
    std::string_view port;
    bool has_port = false;
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string_view::npos || !firmware_url_detail::IsIpv6(authority.substr(1, close - 1))) return false;
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':') return false;
            port = authority.substr(close + 2);
            has_port = true;
        }
    } else {
        const auto colon = authority.find(':');
        if (!firmware_url_detail::IsHost(authority.substr(0, colon))) return false;
        if (colon != std::string_view::npos) {
            port = authority.substr(colon + 1);
            has_port = true;
        }
    }
    if (has_port) {
        if (port.empty() || port.size() > 5) return false;
        unsigned value = 0;
        for (char c : port) {
            if (c < '0' || c > '9') return false;
            value = value * 10 + static_cast<unsigned>(c - '0');
        }
        if (value == 0 || value > 65535) return false;
    }
    return true;
}

// Construct only after the separate replacement-firmware confirmation.
class FirmwareInstallScope {
public:
    FirmwareInstallScope() { SetFirmwareInstallAllowed(true); }
    ~FirmwareInstallScope() { SetFirmwareInstallAllowed(false); }
    FirmwareInstallScope(const FirmwareInstallScope&) = delete;
    FirmwareInstallScope& operator=(const FirmwareInstallScope&) = delete;
};
}  // namespace stock_services
