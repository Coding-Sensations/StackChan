#pragma once
#include "numeric_pairing.h"
namespace companion {
// One physical matching-code confirmation authorizes only its scoped grant.
// No transport/device IO. The caller requires staged ACK before arming.
class PairConnectionConsent {
    uint64_t deadline_=0; bool pending_=false,connection_requested_=false;
public:
    bool Arm(const std::string& request,uint64_t deadline,bool audio_requested,bool physically_confirmed,uint64_t now) {
        Cancel(); if(!PairHex(request)||!audio_requested||!physically_confirmed||deadline<=now||deadline-now>120000)return false;
        deadline_=deadline;pending_=true;return true;
    }
    bool RequestConnection(bool host_ready) {
        if(!pending_||connection_requested_||!host_ready)return false;
        connection_requested_=true;return true;
    }
    bool Take(uint64_t now,bool session_requires_consent) {
        if(!pending_||!connection_requested_)return false;
        if(now>=deadline_){Cancel();return false;}
        if(!session_requires_consent)return false;
        Cancel();return true;
    }
    bool Pending()const{return pending_;}
    void Cancel(){deadline_=0;pending_=connection_requested_=false;}
};
}
