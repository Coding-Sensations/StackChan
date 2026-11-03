#pragma once
#include <string>
#include <cstdint>
#include <lvgl.h>
#include "saved_wifi_owner.hpp"
#include "wifi_survey.hpp"
namespace companion {
class FeatureLink;
struct AuthorizedGrant { std::string uri, device, credential, request; bool host_audio_qualified=false; uint64_t expires_at_ms=0; bool numeric_pairing=false; FeatureLink* feature_authority=nullptr; };
// Explicit trusted caller only. No network/serial provisioning listener, token issuance or NVS.
bool StageAuthorizedGrant(AuthorizedGrant grant);
// Joint-view trusted handoff: consumes only the link's real physically confirmed union grant.
// Staging stays muted. Connect and Listen remain separate explicit user actions.
bool StageFeatureAudioGrant(FeatureLink&,uint64_t now);
bool ConnectFeatureAudio(FeatureLink&,const std::string& request);
bool ListenFeatureAudio(FeatureLink&,const std::string& request,uint32_t epoch);
uint32_t FeatureAudioEpoch(const FeatureLink&);
bool FeatureAudioStaged(const FeatureLink&);
bool FeatureAudioIdle();
void OpenView(lv_obj_t* parent);
void TickView();
bool CloseView(); // false: retain app/UI while codec worker stops
[[noreturn]] void RunBoot();
}

namespace companion {
// Explicit saved-Wi-Fi connection for a user-requested catalog browse, no cloud/audio.
bool EnsureSavedNetwork();
// Main-owner polling only: stop new starts/borrows, wait consumers/scan, then
// unregister callbacks and stop the retained station on a worker without LVGL.
bool PollSavedNetworkStopped();
bool IsWifiConnected();
bool NetworkWasStarted();
SavedWifiPhase SavedNetworkPhase();
uint16_t SavedNetworkDisconnectReason();
}

namespace companion {
bool RequestWifiSurvey();
void PollWifiSurvey();
bool CancelWifiSurvey();
survey::Snapshot WifiSurveySnapshot();
// Small state read and caller-owned copy avoid multi-kilobyte UI stack temporaries.
survey::Phase WifiSurveyPhase();
void CopyWifiSurveySnapshot(survey::Snapshot& output);
}

namespace companion { bool RememberedStartAllowed(); }

namespace companion {
// Presentation-only routing for the wrapper; no owner, capture or pairing action.
bool CompanionCharacterVisible();
void ShowMiraCharacter();
}
