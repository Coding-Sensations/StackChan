#pragma once
#include "toolbox_models.hpp"
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#include <atomic>
#include <memory>
namespace stock_tools {
class Infrared {
    rmt_channel_handle_t rx_ = nullptr;
    std::array<rmt_symbol_word_t, 256> symbols_{};
    std::atomic<size_t> received_{0};
    uint64_t until_ = 0;

public:
    std::vector<uint32_t> learned;
    uint32_t carrier_hz = 38000; // Raw learning cannot measure carrier; known capture assumption.
    bool Learn(uint64_t now);
    bool Poll(uint64_t now);
    bool SendOnce();
    void Stop();
    bool Active() const
    {
        return rx_ != nullptr;
    }
};
class NfcUrl {
    struct Impl;
    Impl* impl_ = nullptr;  // boot-lifetime owner, retains bus adapter after Close
public:
    bool Start(const std::string& url, uint64_t now);
    bool Stop();
    bool Poll(uint64_t now);
    bool Active() const;
};
}  // namespace stock_tools
