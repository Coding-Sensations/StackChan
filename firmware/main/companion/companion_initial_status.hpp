#pragma once
namespace companion {
// Presentation only. Never infer a live handshake from opening the page.
inline const char* InitialViewStatus(bool reconnect_enabled,const char* live_status){
    if(!reconnect_enabled)return "Mira companion\nSaved reconnect disabled in this build\nCapture unverified; camera/motion inactive";
    if(live_status&&*live_status)return live_status;
    return "Mira companion\nSaved reconnect unavailable\nCapture unverified; camera/motion inactive";
}
}
