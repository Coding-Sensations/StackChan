// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "voice_exchange_fixture.hpp"
#include <companion/muse_voice_session_worker.hpp>
#include <deque>
#include <functional>
using companion::muse::VoiceSessionWorker;
struct Clock:muse_native_review::Clock {
 int64_t mono=0,wall=1760000000000LL;int samples=0;bool okay=true;std::function<void()> hook;std::function<void(int)> sample_hook;
 bool Sample(muse_native_review::Stamp& s)override{++samples;if(sample_hook)sample_hook(samples);if(hook){auto f=std::move(hook);hook={};f();}s={mono,wall};return okay;}
};
struct Custody:muse_native_review::Custody {
 muse_native_review::Budget budget{7,10000000,1760000010000LL};bool current=true;
 bool ReadBudget(muse_native_review::Budget& b)override{b=budget;return true;}
 bool Current(const muse_native_review::Budget& b)override{return current&&b.generation==budget.generation&&b.expires_us==budget.expires_us&&b.expires_wall_ms==budget.expires_wall_ms;}
};
struct Socket {
 bool verified=true,send_ok=true,close_ok=true;int sends=0,receives=0,closes=0;int receive_error=0;uint8_t* receive_buffer=nullptr;size_t receive_capacity=0;
 std::deque<std::vector<uint8_t>> incoming;std::vector<std::vector<uint8_t>> sent;
 std::function<void()> send_hook,receive_hook,close_hook;
 bool VerifiedMetaEndpoint()const{return verified;}
 bool Send(const uint8_t* p,size_t n,unsigned timeout){assert(timeout&&timeout<=100);++sends;sent.emplace_back(p,p+n);if(send_hook)send_hook();return send_ok;}
 int Receive(uint8_t* p,size_t capacity,unsigned timeout){assert(timeout&&timeout<=20);++receives;receive_buffer=p;receive_capacity=capacity;if(receive_hook)receive_hook();if(receive_error)return receive_error;if(incoming.empty())return 0;auto packet=std::move(incoming.front());incoming.pop_front();assert(packet.size()<=capacity);std::memcpy(p,packet.data(),packet.size());return int(packet.size());}
 bool CloseAndJoin(unsigned timeout){assert(timeout<=20);++closes;if(close_hook)close_hook();return close_ok;}
};
using Worker=VoiceSessionWorker<Socket>;
