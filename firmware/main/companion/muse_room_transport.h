#pragma once
class NetworkInterface;
namespace companion::room {
struct MuseRoomGrant;
// No automatic startup or enrollment. An explicitly approved trusted owner
// supplies this volatile grant while Muse is selected. No credentials stored.
// Start/Poll/Stop belong to the same serialized UI lifecycle task. A foreign
// stop only revokes; the retained owner must poll to join transport cleanup.
bool StartMuseRoom(NetworkInterface& network,const MuseRoomGrant& grant);
// Staging performs no IO. Only the Muse Settings Connect action consumes it.
// The trusted owner retains the NetworkInterface and a live, independently
// approved network_current lease check until joined cleanup. No Mira lease is
// acquired, changed or inferred by this module.
bool PrepareMuseRoom(NetworkInterface& network,const MuseRoomGrant& grant);
bool MuseRoomPrepared();
bool ConnectPreparedMuseRoom();
void PollMuseRoom();
void StopMuseRoom();
bool MuseRoomAuthenticated();
bool MuseRoomCleanupComplete();
}
