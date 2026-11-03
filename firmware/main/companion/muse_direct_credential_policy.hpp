// SPDX-License-Identifier: Apache-2.0
#pragma once
#ifndef STACKCHAN_MUSE_RAM_ONLY
#define STACKCHAN_MUSE_RAM_ONLY 0
#endif
namespace companion::muse {
#if STACKCHAN_MUSE_RAM_ONLY
inline constexpr const char* StorageConsent="Hold token in RAM until Muse exit/reboot?";
inline constexpr const char* PreparedHelp="Start explicitly in Settings. RAM token lost on Muse exit/reboot.";
inline constexpr const char* CredentialReadConsent="Allow RAM credential read?";
#else
inline constexpr const char* StorageConsent="Save device tokens unencrypted?";
inline constexpr const char* PreparedHelp="Start explicitly in Settings first.";
inline constexpr const char* CredentialReadConsent="Allow saved credential read?";
#endif
}
