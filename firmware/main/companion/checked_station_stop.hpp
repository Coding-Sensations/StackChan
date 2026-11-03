#pragma once
namespace companion::network {
// Call only after consumers and scans have joined and new acquisition is gated.
// Each checked step retains failed resources for the same owner's retry.
template<class Driver> bool StopStationChecked(Driver& driver){
    if(!driver.UnregisterWifi())return false;
    if(!driver.UnregisterIp())return false;
    if(!driver.StopDriver())return false;
    if(!driver.DrainEvents())return false;
    if(!driver.DestroyNetif())return false;
    if(!driver.DrainEvents())return false;
    return driver.VerifyIdle();
}
}
