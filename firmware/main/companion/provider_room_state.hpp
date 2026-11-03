#pragma once
#include <cstdint>
#include <string>
namespace companion::room {
// Local, explicitly sourced conversation state. Existing authenticated room
// packets do not carry this field, so their default remains inactive.
enum class Communication {Idle,Listening,TextPending,Speaking};
struct Snapshot {
  Communication communication=Communication::Idle;
  bool local_communication_current=false; // Local checked-IO facts only; not a wire field.
  std::string scene = "unknown", summary, request, operation, ticket;
  // Optional Muse-only display excerpt; never an approval summary.
  std::string caption;
  uint64_t sequence = 0;
  uint32_t ttl = 0;
  bool verified = false;
};
} // namespace companion::room
