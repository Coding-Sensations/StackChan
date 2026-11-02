// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "service_exchange.hpp"
namespace voice_prep {
struct ExchangeApi {
 unsigned version;size_t bytes,alignment;
 ServiceExchange* (*construct)(void*,size_t,Protocol*,musegadgets::noise::core::ClientSession*);
 bool (*destroy)(ServiceExchange*);
 decltype(&ServiceExchange::Begin) begin;
 decltype(&ServiceExchange::Queue) queue;
 decltype(&ServiceExchange::Next) next;
 decltype(&ServiceExchange::Written) written;
 decltype(&ServiceExchange::Receive) receive;
 decltype(&ServiceExchange::Cancel) cancel;
 decltype(&ServiceExchange::Abort) abort;
 decltype(&ServiceExchange::Joined) joined;
 decltype(&ServiceExchange::NeedsTransportClose) needs_close;
 decltype(&ServiceExchange::CleanupComplete) cleanup_complete;
};
}
extern "C" const voice_prep::ExchangeApi* stackchan_muse_voice_exchange_api();
