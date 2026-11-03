#pragma once
namespace companion {
// Per-grant physical Connect consent survives Wi-Fi setup, never Mute/pairing.
class ConnectGate {
    bool authorized_=false;
public:
    void Cancel() { authorized_=false; }
    void Request(bool grant_present) { authorized_=grant_present; }
    bool Ready(bool wifi_connected) const { return authorized_ && wifi_connected; }
};
}
