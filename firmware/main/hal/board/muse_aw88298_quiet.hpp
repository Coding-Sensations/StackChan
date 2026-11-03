// SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include <audio_codec_if.h>
#include <cstdint>
namespace codec_quiet {
// Register policy: esp_codec_dev1.5.4, esp-adf fcce23e3, AW88298 driver.
// Explicit RMW avoids cached enable(false) skipping the shutdown write and
// high-level mute/close wrappers discarding lower-level errors. No acoustic claim.
enum class Stage{None,ReadMute,WriteMute,ReadSystem,WriteSystem,SyncDisable,VerifyMute,VerifySystem,Complete};
enum class Fault{None,Unavailable,DriverError,BindingLost,ReadbackMismatch};
struct Evidence{
 Stage stage=Stage::None;Fault fault=Fault::Unavailable;int status=0;uint32_t attempted=0;
 int mute_before=0,system_before=0,mute_after=0,system_after=0;
 bool Complete()const{return stage==Stage::Complete&&fault==Fault::None;}
};
inline bool Available(const audio_codec_if_t*p){return p&&p->get_reg&&p->set_reg&&p->enable;}
template<class Current,class Call>bool CallAt(Evidence&e,Stage stage,Current&current,Call call){
 e.stage=stage;if(!current()){e.fault=Fault::BindingLost;return false;}
 e.attempted|=uint32_t(1)<<unsigned(stage);e.status=call();
 if(e.status!=0){e.fault=Fault::DriverError;return false;}
 if(!current()){e.fault=Fault::BindingLost;return false;}return true;
}
template<class Current>Evidence Verify(const audio_codec_if_t*p,Current current,Evidence e){
 if(!Available(p)||e.fault!=Fault::None){e.fault=Fault::Unavailable;return e;}
 if(!CallAt(e,Stage::VerifyMute,current,[&]{return p->get_reg(p,5,&e.mute_after);}))return e;
 if(e.mute_after!=(e.mute_before|0x10)){e.fault=Fault::ReadbackMismatch;return e;}
 if(!CallAt(e,Stage::VerifySystem,current,[&]{return p->get_reg(p,4,&e.system_after);}))return e;
 if(e.system_after!=((e.system_before|0x03)&~0x40)){e.fault=Fault::ReadbackMismatch;return e;}
 e.stage=Stage::Complete;return e;
}
template<class Current>Evidence Quiesce(const audio_codec_if_t*p,Current current){
 Evidence e;if(!Available(p))return e;e.fault=Fault::None;
 if(!CallAt(e,Stage::ReadMute,current,[&]{return p->get_reg(p,5,&e.mute_before);}))return e;
 if(e.mute_before<0||e.mute_before>0xffff){e.fault=Fault::ReadbackMismatch;return e;}
 if(!CallAt(e,Stage::WriteMute,current,[&]{return p->set_reg(p,5,e.mute_before|0x10);}))return e;
 if(!CallAt(e,Stage::ReadSystem,current,[&]{return p->get_reg(p,4,&e.system_before);}))return e;
 if(e.system_before<0||e.system_before>0xffff){e.fault=Fault::ReadbackMismatch;return e;}
 if(!CallAt(e,Stage::WriteSystem,current,[&]{return p->set_reg(p,4,(e.system_before|0x03)&~0x40);}))return e;
 // Synchronize the driver's enabled cache too, so a later distinct owner can
 // open normally. A cached no-op cannot replace the RMW/readback above.
 if(!CallAt(e,Stage::SyncDisable,current,[&]{return p->enable(p,false);}))return e;
 return Verify(p,current,e);
}
}
#endif
