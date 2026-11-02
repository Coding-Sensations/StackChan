/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Test-only nonce-tag stand-in from the pinned SDK harness. NOT cryptographic verification.
#pragma once
#include <xplat/noise/core/ClientSession.h>
#include <cassert>
#include <cstring>
namespace tn = musegadgets::noise::core;
class NonceTagCrypto : public tn::CryptoBackend {
 public:
  tn::Status fill(tn::ByteSpan out) noexcept {
    if (out.size()) std::memset(out.data(), 0x42, out.size());
    return tn::OkStatus();
  }
  tn::Status Random(tn::ByteSpan out) noexcept override { return fill(out); }
  tn::Status X25519GenerateKeypair(tn::ByteSpan priv, tn::ByteSpan pub) noexcept override {
    (void)fill(priv);
    return fill(pub);
  }
  tn::Status X25519PublicFromPrivate(tn::ConstByteSpan, tn::ByteSpan out) noexcept override {
    return fill(out);
  }
  tn::Status X25519Dh(tn::ConstByteSpan, tn::ConstByteSpan, tn::ByteSpan out) noexcept override {
    return fill(out);
  }
  tn::Status Sha256(tn::ConstByteSpan, tn::ByteSpan out) noexcept override { return fill(out); }
  tn::Status Sha256Concat(tn::ConstByteSpan, tn::ConstByteSpan, tn::ByteSpan out) noexcept override {
    return fill(out);
  }
  tn::Status HkdfSha256(tn::ConstByteSpan, tn::ConstByteSpan, tn::ConstByteSpan,
                        tn::Span<tn::ByteSpan> out) noexcept override {
    for (size_t i = 0; i < out.size(); ++i) (void)fill(out[i]);
    return tn::OkStatus();
  }
  // Ciphertext = plaintext; tag = the 12-byte nonce and four zero bytes.
  tn::Status Aes256GcmSeal(tn::ConstByteSpan, tn::ConstByteSpan nonce, tn::ConstByteSpan,
                           tn::ConstByteSpan in, tn::ByteSpan out) noexcept override {
    if (out.size() < in.size() + kAes256GcmTagSize) return tn::Status::ResourceExhausted();
    if (in.size()) std::memmove(out.data(), in.data(), in.size());
    std::memset(out.data() + in.size(), 0, kAes256GcmTagSize);
    std::memcpy(out.data() + in.size(), nonce.data(), nonce.size());
    return tn::OkStatus();
  }
  tn::Status Aes256GcmOpen(tn::ConstByteSpan, tn::ConstByteSpan nonce, tn::ConstByteSpan,
                           tn::ConstByteSpan in, tn::ByteSpan out) noexcept override {
    if (in.size() < kAes256GcmTagSize) return tn::Status::InvalidArgument();
    size_t size = in.size() - kAes256GcmTagSize;
    if (std::memcmp(in.data() + size, nonce.data(), nonce.size()) != 0) {
      return tn::Status::DataLoss();   // wrong nonce: the "tag" doesn't verify
    }
    if (out.size() < size) return tn::Status::ResourceExhausted();
    if (size) std::memmove(out.data(), in.data(), size);
    return tn::OkStatus();
  }
};

void Establish(tn::ClientSession& s) {
  uint8_t handshake[256] = {};
  assert(s.WriteHandshakeMessage1(tn::ByteSpan(handshake)).ok());
  size_t extra = 0;
  assert(s.ReadHandshakeMessage2(
              tn::ConstByteSpan(handshake, tn::InitiatorHandshake::kMinMessage2Size),
              tn::ByteSpan(), extra)
             .ok());
  assert(s.WriteHandshakeMessage3(tn::ConstByteSpan(), tn::ByteSpan(handshake)).ok());
  assert(s.isEstablished());
}
