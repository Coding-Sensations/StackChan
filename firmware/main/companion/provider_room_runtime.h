#pragma once
#include "provider_room_state.hpp"
#include <apps/stock_shell/provider_catalog.hpp>
#include <cstddef>
namespace companion::room {
using Provider = companion_modes::Provider;
void SelectProvider(Provider provider);
bool ProviderSelected(Provider provider);
// Privileged separately authorized source-owner API. Audio pairing is not a
// grant.
bool BindProvider(Provider provider, const std::string &binding,
                  const std::string &generation, const unsigned char *event,
                  size_t es, const unsigned char *decision, size_t ds,
                  bool approval_scope);
bool AcceptProvider(Provider provider, const std::string &wire, uint64_t now);
Snapshot ViewProvider(Provider provider, uint64_t now);
bool QueueDecision(Provider provider, const std::string &choice,
                   const std::string &captured_ticket, uint64_t now);
std::string TakeDecision(Provider provider);
void DisconnectProvider(Provider provider);
} // namespace companion::room
