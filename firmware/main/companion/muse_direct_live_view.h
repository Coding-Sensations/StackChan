// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <string>
namespace companion::muse {
enum class SessionStage{Off,Prepared,Starting,Ready,ReplyEnded,Closing};
struct SessionSnapshot{SessionStage stage=SessionStage::Off;std::string caption;bool reply_ended=false;bool communicating=false;bool response_complete=false;};
bool NativeSessionPrepared();bool StartPreparedNativeSession();SessionSnapshot ViewNativeSession();
}
