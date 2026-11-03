// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_session.hpp"
#include "muse_direct_noise.hpp"
#include "muse_direct_audio.hpp"
namespace companion::muse {
// Internal trusted-owner ABI, not a USB/BLE/remote command handler. All explicit
// grants and port lifetimes must be retained by the caller through checked join.
struct RuntimeApi {
    PairingSession* (*pairing)(CredentialStore&,DeviceVerifier&);
    NoiseChatOwner* (*chat)(NoiseSocket&);
    wire::CryptoBackend* (*crypto)();
    CredentialStore* (*store)();
    DeviceVerifier* (*verifier)(std::function<bool()>);
    decltype(&PairingSession::Open) open_pairing;
    decltype(&PairingSession::Receive) receive_pairing;
    decltype(&PairingSession::PhysicalPress) physical_press;
    decltype(&PairingSession::Close) close_pairing;
    decltype(&NoiseChatOwner::Open) open_chat;
    decltype(&NoiseChatOwner::Poll) poll_chat;
    decltype(&NoiseChatOwner::Submit) submit_text;
    decltype(&NoiseChatOwner::Close) close_chat;
    NoiseSocket* (*socket)(std::function<bool()>);
    size_t socket_ram;
    size_t pairing_ram,chat_ram,chat_buffers;
};
}
extern "C" const companion::muse::RuntimeApi* stackchan_muse_direct_runtime();
