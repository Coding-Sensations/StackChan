// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_admission.hpp"
#include "muse_direct_live_view.h"
namespace companion::muse {
// Trusted local-owner API, not a token/USB/remote receiver. All grant callbacks
// and their contexts remain retained through a true cleanup receipt.
void SessionWorkerProviderVisible(bool);
bool PrepareNativeSession(const SessionGrant&);
bool NativeSessionPrepared();
bool StartPreparedNativeSession();
bool QueueNativeText(const std::string& captured_generation,const std::string& request,const std::string& text);
SessionSnapshot ViewNativeSession();
void StopNativeSession();
bool NativeSessionCleanupComplete();
}
namespace companion::muse {
struct SessionWorkerApi {
 decltype(&PrepareNativeSession) prepare;decltype(&StartPreparedNativeSession) start;
 decltype(&QueueNativeText) text;decltype(&ViewNativeSession) view;
 decltype(&StopNativeSession) stop;decltype(&NativeSessionCleanupComplete) cleanup;
 size_t working_set_ram,window_ram,text_packet_ram;
};
}
extern "C" const companion::muse::SessionWorkerApi* stackchan_muse_direct_session_worker();
