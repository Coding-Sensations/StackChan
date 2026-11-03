// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <audio_codec_if.h>
using Codec=CoreS3AudioCodec;using Outcome=Codec::MuseQuiesce;using Phase=Codec::MuseQuiescePhase;
static int mute_reg=8,system_reg=0x4040,amp_calls=0,amp_reads=0,amp_writes=0,amp_disables=0,fail_at=0,fail_status=-73,corrupt_at=0;
static bool cached_enabled=false;static std::function<void(int)> amp_hook;
static void AmpHook(){if(amp_hook){auto hook=amp_hook;hook(amp_calls);}}
static int Get(const audio_codec_if_t*,int reg,int*out){++amp_calls;++amp_reads;assert(reg==4||reg==5);if(amp_calls==fail_at)return fail_status;*out=reg==4?system_reg:mute_reg;if(amp_calls==corrupt_at)*out^=reg==4?0x40:0x10;AmpHook();return 0;}
static int Set(const audio_codec_if_t*,int reg,int value){++amp_calls;++amp_writes;if(amp_calls==fail_at)return fail_status;if(reg==4)system_reg=value;else{assert(reg==5);mute_reg=value;}AmpHook();return 0;}
static int Disable(const audio_codec_if_t*,bool enable){++amp_calls;++amp_disables;assert(!enable);if(amp_calls==fail_at)return fail_status;cached_enabled=false;AmpHook();return 0;} // cached false may skip all register writes
static audio_codec_if_t amplifier{Get,Set,Disable};
