#pragma once
#include "usb_pcm_runtime.h"
#include <string>
namespace direct_usb_pcm {
struct Selection {std::string request;uint32_t boot=0,epoch=0;uint16_t stream=0;uint64_t cutoff=0;};
struct View {RuntimeState state=RuntimeState::Unselected;SetupIssue issue=SetupIssue::None;std::string code,stamp;Snapshot audio;bool closed=true,configured=false;};
// Trusted future provisioning/controller calls this with its selected request.
// It is not media admission, does not open USB, and is absent by default.
bool StageSelection(const Selection&);
void ReportSetupIssue(SetupIssue);
void SelectFromPage();void ConfirmFromPage(const std::string&,const std::string&);void StopFromPage();
View ReadView();
}
