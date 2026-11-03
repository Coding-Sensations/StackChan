#include "provider_room_runtime.h"
#include "provider_room_wire.hpp"
#include <memory>
#include <mutex>
namespace companion::room {
namespace {
std::mutex mutex;
std::unique_ptr<Wire> contexts[6];
Provider selected = Provider::Mira;
std::string pending;
int Index(Provider p) {
  int n = int(p);
  return n >= 0 && n < 6 ? n : -1;
}
Wire *Get(Provider p) {
  int n = Index(p);
  if (n < 0)
    return nullptr;
  if (!contexts[n]) {
    const char *names[] = {"stackchan", "mira",   "hermes",
                           "muse",      "claude", "codex"};
    contexts[n] = std::make_unique<Wire>(names[n]);
  }
  return contexts[n].get();
}
} // namespace
void SelectProvider(Provider provider) {
  std::lock_guard<std::mutex> lock(mutex);
  if (Index(provider) < 0)
    return;
  if (provider != selected) {
    if (auto old = Get(selected))
      old->Disconnect();
    pending.clear();
    selected = provider;
  }
}
bool ProviderSelected(Provider p) {
  std::lock_guard<std::mutex> lock(mutex);
  return Index(p) >= 0 && p == selected;
}
bool BindProvider(Provider p, const std::string &binding,
                  const std::string &generation, const unsigned char *e,
                  size_t es, const unsigned char *d, size_t ds, bool approval) {
  std::lock_guard<std::mutex> lock(mutex);
  auto wire = Get(p);
  return p == selected && wire &&
         wire->Bind(binding, generation, e, es, d, ds, approval);
}
bool AcceptProvider(Provider p, const std::string &input, uint64_t now) {
  std::lock_guard<std::mutex> lock(mutex);
  auto wire = Get(p);
  return p == selected && wire && wire->Accept(input, now);
}
Snapshot ViewProvider(Provider p, uint64_t now) {
  std::lock_guard<std::mutex> lock(mutex);
  auto wire = Get(p);
  return p == selected && wire ? wire->Current(now) : Snapshot{};
}
bool QueueDecision(Provider p, const std::string &choice,
                   const std::string &ticket, uint64_t now) {
  std::lock_guard<std::mutex> lock(mutex);
  auto wire = Get(p);
  if (p != selected || !wire || !pending.empty())
    return false;
  pending = wire->Decision(choice, ticket, now);
  return !pending.empty();
}
std::string TakeDecision(Provider p) {
  std::lock_guard<std::mutex> lock(mutex);
  if (p != selected)
    return {};
  auto result = std::move(pending);
  pending.clear();
  return result;
}
void DisconnectProvider(Provider p) {
  std::lock_guard<std::mutex> lock(mutex);
  if (auto wire = Get(p))
    wire->Disconnect();
  if (p == selected)
    pending.clear();
}
} // namespace companion::room
