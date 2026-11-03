#pragma once
#include <cstdint>
namespace companion {
// A saved peer may coexist with a later explicit legacy/FeatureLink grant.
// Presence alone never steals that grant's physical Connect or Listen action.
inline bool RouteRememberedConnect(bool present,bool pending_grant,bool feature_staged,bool legacy_connected,bool numeric_enrollment){
 return present&&!pending_grant&&!feature_staged&&!legacy_connected&&!numeric_enrollment;
}
inline bool RouteRememberedStatus(bool present,bool pending_grant,bool feature_staged,bool legacy_connected,bool numeric_enrollment){
 return RouteRememberedConnect(present,pending_grant,feature_staged,legacy_connected,numeric_enrollment);
}
inline bool RouteRememberedListen(uint64_t ready_generation,bool legacy_connected){return ready_generation!=0&&!legacy_connected;}
}
