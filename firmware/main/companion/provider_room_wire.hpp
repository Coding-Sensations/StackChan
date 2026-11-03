#pragma once
#include "provider_room_state.hpp"
#include <array>
#include <cJSON.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mbedtls/md.h>
#include <string>
namespace companion::room {
inline bool Id(const std::string &s) {
  if (s.empty() || s.size() > 64)
    return false;
  for (unsigned char c : s)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
          c == ':'))
      return false;
  return true;
}
inline bool Digest(const std::string &s) {
  if (s.size() != 64)
    return false;
  for (char c : s)
    if (!((c >= 'a' && c <= 'f') || (c >= '0' && c <= '9')))
      return false;
  return true;
}
inline std::string Mac(const unsigned char *key, size_t size,
                       const char *domain, const std::string &body) {
  unsigned char output[32]{};
  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  auto info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  int result = info ? mbedtls_md_setup(&context, info, 1) : -1;
  if (!result)
    result = mbedtls_md_hmac_starts(&context, key, size);
  if (!result)
    result = mbedtls_md_hmac_update(
        &context, reinterpret_cast<const unsigned char *>(domain),
        std::strlen(domain) + 1);
  if (!result)
    result = mbedtls_md_hmac_update(
        &context, reinterpret_cast<const unsigned char *>(body.data()),
        body.size());
  if (!result)
    result = mbedtls_md_hmac_finish(&context, output);
  mbedtls_md_free(&context);
  if (result)
    return {};
  const char *digits = "0123456789abcdef";
  std::string out(64, '0');
  for (size_t n = 0; n < 32; ++n) {
    out[n * 2] = digits[output[n] >> 4];
    out[n * 2 + 1] = digits[output[n] & 15];
  }
  return out;
}
class Wire {
  std::string provider_, binding_, generation_, last_generation_, spent_ticket_;
  std::array<unsigned char, 64> event_{}, decision_{};
  size_t event_size_ = 0, decision_size_ = 0;
  bool bound_ = false, approve_ = false, spent_ = false;
  uint64_t received_ = 0, decision_sequence_ = 0;
  Snapshot snapshot_;
  static std::string Text(cJSON *p, const char *name) {
    auto v = cJSON_GetObjectItemCaseSensitive(p, name);
    return cJSON_IsString(v) && v->valuestring ? std::string(v->valuestring)
                                               : std::string();
  }
  static bool Number(cJSON *p, const char *name, uint64_t limit,
                     uint64_t &out) {
    auto v = cJSON_GetObjectItemCaseSensitive(p, name);
    if (!cJSON_IsNumber(v) || !std::isfinite(v->valuedouble) ||
        v->valuedouble < 0 || v->valuedouble > double(limit) ||
        std::floor(v->valuedouble) != v->valuedouble)
      return false;
    out = uint64_t(v->valuedouble);
    return true;
  }
  void Wipe() {
    for (volatile unsigned char &c : event_)
      c = 0;
    for (volatile unsigned char &c : decision_)
      c = 0;
    event_size_ = decision_size_ = 0;
  }

public:
  explicit Wire(std::string provider) : provider_(std::move(provider)) {}
  ~Wire() { Wipe(); }
  Wire(const Wire &) = delete;
  Wire &operator=(const Wire &) = delete;
  // Privileged owner API, never populated from a received packet. Enrollment
  // must provide a fresh unique generation and separate authorized
  // activity/decision keys.
  bool Bind(std::string binding, std::string generation,
            const unsigned char *event, size_t es,
            const unsigned char *decision, size_t ds, bool approval_scope) {
    if (bound_ || !Id(binding) || !Id(generation) ||
        generation == last_generation_ || !event || !decision || es < 32 ||
        es > 64 || ds < 32 || ds > 64 ||
        (es == ds && !std::memcmp(event, decision, es)))
      return false;
    binding_ = std::move(binding);
    generation_ = std::move(generation);
    last_generation_ = generation_;
    std::memcpy(event_.data(), event, es);
    std::memcpy(decision_.data(), decision, ds);
    event_size_ = es;
    decision_size_ = ds;
    bound_ = true;
    approve_ = approval_scope;
    spent_ = false;
    spent_ticket_.clear();
    received_ = decision_sequence_ = 0;
    snapshot_ = {};
    return true;
  }
  void Disconnect() {
    bound_ = approve_ = spent_ = false;
    binding_.clear();
    generation_.clear();
    spent_ticket_.clear();
    snapshot_ = {};
    Wipe();
  }
  bool Accept(const std::string &wire, uint64_t now) {
    if (!bound_ || wire.size() < 67 || wire.size() > 2048 || wire[64] != '\n' ||
        !Digest(wire.substr(0, 64)))
      return false;
    auto body = wire.substr(65);
    if (body.find("\\u0000") != std::string::npos)
      return false;
    auto tag =
        Mac(event_.data(), event_size_, "stackchan-provider-room-v1", body);
    if (tag.size() != 64)
      return false;
    unsigned different = 0;
    for (size_t n = 0; n < 64; ++n)
      different |= unsigned(wire[n] ^ tag[n]);
    if (different)
      return false;
    const char *end = nullptr;
    cJSON *raw =
        cJSON_ParseWithLengthOpts(body.c_str(), body.size() + 1, &end, true);
    if (!cJSON_IsObject(raw)) {
      cJSON_Delete(raw);
      return false;
    }
    struct Owner {
      cJSON *p;
      ~Owner() { cJSON_Delete(p); }
    } owner{raw};
    const char *fields[] = {"version",         "provider",   "binding",
                            "generation",      "sequence",   "scene",
                            "summary",         "request_id", "operation_sha256",
                            "presentation_id", "ttl_ms", "caption"};
    bool seen[12]{};
    unsigned count = 0;
    for (auto child = raw->child; child; child = child->next) {
      int index = -1;
      for (int n = 0; n < 12; ++n)
        if (child->string && !std::strcmp(child->string, fields[n]))
          index = n;
      if (index < 0 || seen[index])
        return false;
      seen[index] = true;
      ++count;
    }
    if (count != 11 && count != 12)
      return false;
    for (int n = 0; n < 11; ++n)
      if (!seen[n])
        return false;
    if (seen[11] && (provider_ != "muse" ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(raw, "caption"))))
      return false;
    uint64_t version = 0, sequence = 0, ttl = 0;
    if (!Number(raw, "version", 1, version) || version != 1 ||
        !Number(raw, "sequence", 9007199254740991ULL, sequence) ||
        sequence <= snapshot_.sequence || !Number(raw, "ttl_ms", 60000, ttl))
      return false;
    for (const char *name :
         {"provider", "binding", "generation", "scene", "summary", "request_id",
          "operation_sha256", "presentation_id"})
      if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(raw, name)))
        return false;
    if (Text(raw, "provider") != provider_ ||
        Text(raw, "binding") != binding_ ||
        Text(raw, "generation") != generation_)
      return false;
    Snapshot next;
    next.sequence = sequence;
    next.verified = true;
    next.scene = Text(raw, "scene");
    next.summary = Text(raw, "summary");
    next.request = Text(raw, "request_id");
    next.operation = Text(raw, "operation_sha256");
    next.ticket = Text(raw, "presentation_id");
    next.ttl = uint32_t(ttl);
    if (seen[11]) {
      next.caption = Text(raw, "caption");
      if (next.caption.empty() || next.caption.size() > 160 ||
          next.scene == "decision_sign" || next.scene == "unknown")
        return false;
      for (unsigned char c : next.caption)
        if (c < 32 || c > 126)
          return false;
    }
    bool known = false;
    for (const char *name :
         {"unknown", "desk_work", "decision_sign", "error", "completion",
          "idle_decoration", "waiting_receipt"})
      known |= next.scene == name;
    if (!known || next.summary.size() > 160)
      return false;
    for (unsigned char c : next.summary)
      if (c < 32 || c > 126)
        return false;
    if (next.scene == "decision_sign") {
      if (!approve_ || !Id(next.request) || !Id(next.ticket) ||
          !Digest(next.operation) || next.summary.empty() || !next.ttl)
        return false;
    } else if (!next.request.empty() || !next.ticket.empty() ||
               !next.operation.empty() || !next.summary.empty() || next.ttl)
      return false;
    // Refreshing an unchanged request must not invalidate a physical press or
    // rearm a locally submitted/expired ticket. A new ticket denotes new
    // custody.
    if (!next.ticket.empty() && next.ticket == snapshot_.ticket &&
        (next.request != snapshot_.request ||
         next.operation != snapshot_.operation ||
         next.summary != snapshot_.summary))
      return false;
    if (snapshot_.scene == "decision_sign" &&
        (now < received_ || now - received_ >= snapshot_.ttl))
      spent_ticket_ = snapshot_.ticket;
    snapshot_ = std::move(next);
    received_ = now;
    spent_ = !snapshot_.ticket.empty() && snapshot_.ticket == spent_ticket_;
    return true;
  }
  Snapshot Current(uint64_t now) {
    if (snapshot_.scene == "decision_sign" &&
        (spent_ || now < received_ || now - received_ >= snapshot_.ttl)) {
      if (!snapshot_.ticket.empty())
        spent_ticket_ = snapshot_.ticket;
      snapshot_.scene = "waiting_receipt";
      snapshot_.request.clear();
      snapshot_.operation.clear();
      snapshot_.ticket.clear();
      snapshot_.summary.clear();
      snapshot_.ttl = 0;
      spent_ = true;
    }
    return snapshot_;
  }
  std::string Decision(const std::string &choice,
                       const std::string &captured_ticket, uint64_t now) {
    const auto current = Current(now);
    if (!bound_ || !approve_ || spent_ || current.scene != "decision_sign" ||
        captured_ticket != current.ticket ||
        (choice != "approve" && choice != "deny") ||
        decision_sequence_ >= 9007199254740990ULL)
      return {};
    spent_ = true;
    spent_ticket_ = current.ticket;
    ++decision_sequence_;
    std::string body =
        "{\"version\":1,\"provider\":\"" + provider_ + "\",\"binding\":\"" +
        binding_ + "\",\"generation\":\"" + generation_ +
        "\",\"sequence\":" + std::to_string(decision_sequence_) +
        ",\"presentation_id\":\"" + current.ticket + "\",\"request_id\":\"" +
        current.request + "\",\"operation_sha256\":\"" + current.operation +
        "\",\"choice\":\"" + choice + "\"}";
    auto mac = Mac(decision_.data(), decision_size_,
                   "stackchan-provider-decision-v1", body);
    if (mac.size() != 64)
      return {};
    return mac + '\n' + body;
  }
};
} // namespace companion::room
