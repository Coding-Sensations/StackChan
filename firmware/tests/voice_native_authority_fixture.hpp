// SPDX-License-Identifier: Apache-2.0
#pragma once
struct BooleanOnly:voice_codec_prep::Permit {
 mutable unsigned calls=0;
 bool VoiceContext(uint64_t,int64_t)const override{++calls;return true;}
 bool Microphone(uint64_t,int64_t)const override{++calls;return true;}
 bool Output(uint64_t,int64_t)const override{++calls;return true;}
 bool Provider(uint64_t,int64_t)const override{++calls;return true;}
 bool Hold(uint64_t,uint64_t,int64_t)const override{++calls;return true;}
};
struct Matching: BooleanOnly {
 muse_review::LocalReceipt expected{};mutable std::function<void()> during;mutable unsigned matches=0;mutable int64_t seen=0;unsigned trigger=1;mutable std::function<void()> operation;bool current=true;
 bool Probe()const{++calls;auto f=std::move(operation);operation={};if(f)f();return current;}
 bool VoiceContext(uint64_t,int64_t)const override{return Probe();}
 bool Microphone(uint64_t,int64_t)const override{return Probe();}
 bool Output(uint64_t,int64_t)const override{return Probe();}
 bool Provider(uint64_t,int64_t)const override{return Probe();}
 bool Hold(uint64_t,uint64_t ticket,int64_t now)const override{seen=now;return ticket==5&&Probe();}
 bool MatchesReviewedContext(const muse_review::Target& t,companion::muse::Directions d,int64_t n)const override{
  ++matches;seen=n;if(matches==trigger){auto f=std::move(during);during={};if(f)f();}const auto&e=expected.target;
  return current&&expected.valid&&d.microphone==expected.microphone&&d.output==expected.speaker&&t.generation==e.generation&&t.expires.mono==e.expires.mono&&t.expires.wall==e.expires.wall&&!std::strcmp(t.device,e.device)&&!std::strcmp(t.call,e.call)&&!std::strcmp(t.session,e.session)&&!std::strcmp(t.thread,e.thread);
 }
};
