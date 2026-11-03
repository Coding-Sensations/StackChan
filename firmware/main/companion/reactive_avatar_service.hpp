#pragma once
#include "audio_owner.hpp"
namespace companion {
// Passive scalar service, implemented by boot-lifetime UI mailbox. Never renders
// from a worker. All callbacks carry immutable exact custody tokens.
bool BindReactiveAvatar(const AudioLease&);
bool ObserveReactiveAvatar(const AudioLease&,uint64_t generation,uint32_t media_epoch,uint32_t current_media_epoch,uint16_t peak,bool playback,uint32_t monotonic_ms);
void UpdateReactiveMediaEpoch(const AudioLease&,uint32_t current_media_epoch);
void StopReactiveAvatar(const AudioLease&);
void ClosedReactiveAvatar(const AudioLease&,bool checked_closed_and_joined);
// Render-only failed Back path, followed by persistent fallback on forced page
// deletion. Detach retains boot-lifetime cleanup custody, never activates audio.
void RenderClosingCompanionView();
void DetachClosingCompanionView();
bool CompanionClosingPending();
}
