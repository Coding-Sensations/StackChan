#include "stock_tools_hw.h"
#include <hal/board/hal_bridge.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <driver/gpio.h>
#include <esp_random.h>
namespace stock_tools {
void Infrared::Stop()
{
    if (rx_) {
        rmt_disable(rx_);
        rmt_del_channel(rx_);
        rx_ = nullptr;
    }
    received_ = 0;
    until_    = 0;
}
bool Infrared::Learn(uint64_t now)
{
    Stop();
    learned.clear();
    rmt_rx_channel_config_t cfg = {};
    cfg.gpio_num                = GPIO_NUM_10;
    cfg.clk_src                 = RMT_CLK_SRC_DEFAULT;
    cfg.resolution_hz           = 1000000;
    cfg.mem_block_symbols       = 64;
    if (rmt_new_rx_channel(&cfg, &rx_) != ESP_OK) return false;
    rmt_rx_event_callbacks_t cb = {};
    cb.on_recv_done             = [](rmt_channel_handle_t, const rmt_rx_done_event_data_t* e, void* ctx) {
        static_cast<Infrared*>(ctx)->received_.store(e->num_symbols);
        return false;
    };
    if (rmt_rx_register_event_callbacks(rx_, &cb, this) != ESP_OK || rmt_enable(rx_) != ESP_OK) {
        Stop();
        return false;
    }
    rmt_receive_config_t capture = {};
    capture.signal_range_min_ns  = 100000;
    capture.signal_range_max_ns  = 15000000;
    if (rmt_receive(rx_, symbols_.data(), sizeof(symbols_), &capture) != ESP_OK) {
        Stop();
        return false;
    }
    until_ = now;
    return true;
}
bool Infrared::Poll(uint64_t now)
{
    if (!rx_) return false;
    if (WindowExpired(now, until_, 10000)) {
        Stop();
        return false;
    }
    auto n = received_.load();
    if (!n) return false;
    bool started = false;
    std::vector<uint32_t> pulses;
    for (size_t i = 0; i < std::min(n, symbols_.size()); ++i) {
        auto s               = symbols_[i];
        uint32_t duration[2] = {s.duration0, s.duration1};
        uint32_t level[2]    = {s.level0, s.level1};
        for (int j = 0; j < 2; ++j) {
            if (!duration[j]) continue;
            if (!started && level[j] == 0) started = true;
            if (started) pulses.push_back(duration[j]);
        }
    }
    if (!pulses.empty() && pulses.back() >= 14000) pulses.pop_back();
    Stop();
    if (n >= symbols_.size() || pulses.empty() || pulses.size() > 511) return false;
    learned = std::move(pulses);
    return true;
}
bool Infrared::SendOnce()
{
    Stop();
    if (learned.empty() || learned.size() > 511) return false;
    uint64_t total = 0;
    for (auto v : learned) {
        if (!v || v > 15000) return false;
        total += v;
    }
    if (total > 180000) return false;
    std::vector<rmt_symbol_word_t> symbols((learned.size() + 1) / 2);
    for (size_t i = 0; i < learned.size(); ++i) {
        auto& s = symbols[i / 2];
        if (i % 2 == 0) {
            s.level0    = 1;
            s.duration0 = learned[i];
        } else {
            s.level1    = 0;
            s.duration1 = learned[i];
        }
    }
    rmt_channel_handle_t tx      = nullptr;
    rmt_encoder_handle_t encoder = nullptr;
    rmt_tx_channel_config_t cfg  = {};
    cfg.gpio_num                 = GPIO_NUM_5;
    cfg.clk_src                  = RMT_CLK_SRC_DEFAULT;
    cfg.resolution_hz            = 1000000;
    cfg.mem_block_symbols        = 64;
    cfg.trans_queue_depth        = 1;
    if (rmt_new_tx_channel(&cfg, &tx) != ESP_OK) return false;
    rmt_carrier_config_t carrier  = {};
    carrier.frequency_hz          = 38000;
    carrier.duty_cycle            = .33f;
    rmt_copy_encoder_config_t enc = {};
    rmt_transmit_config_t send    = {};
    send.loop_count               = 0;
    send.flags.eot_level          = 0;
    bool ok = rmt_apply_carrier(tx, &carrier) == ESP_OK && rmt_new_copy_encoder(&enc, &encoder) == ESP_OK &&
              rmt_enable(tx) == ESP_OK;
    if (ok)
        ok = rmt_transmit(tx, encoder, symbols.data(), symbols.size() * sizeof(rmt_symbol_word_t), &send) == ESP_OK &&
             rmt_tx_wait_all_done(tx, 300) == ESP_OK;
    rmt_disable(tx);
    if (encoder) rmt_del_encoder(encoder);
    rmt_del_channel(tx);
    gpio_set_level(GPIO_NUM_5, 0);
    return ok;
}
struct NfcUrl::Impl {
    m5::unit::UnitUnified units;
    m5::unit::UnitNFC unit;
    m5::nfc::EmulationLayerA emulation{unit};
    std::array<uint8_t, 180> memory{};
    bool registered = false, needs_stop = false, active = false;
    uint64_t until = 0;
};
bool NfcUrl::Start(const std::string& url, uint64_t now)
{
    if (!Stop()) return false;
    if (!impl_) impl_ = new Impl;
    auto& s = *impl_;
    std::array<uint8_t, 7> uid{};
    esp_fill_random(uid.data(), uid.size());
    uid[0] = 4;
    if (!UrlTag(url, uid, s.memory)) return false;
    auto bus = hal_bridge::board_get_i2c_bus();
    if (!bus || i2c_master_probe(bus, 0x50, 20) != ESP_OK) return false;
    auto cfg      = s.unit.config();
    cfg.emulation = true;
    cfg.mode      = m5::nfc::NFC::A;
    cfg.using_irq = false;
    s.unit.config(cfg);
    if (!s.registered) {
        if (!s.units.add(s.unit, bus)) return false;
        s.registered = true;
    }
    s.needs_stop = true;
    m5::nfc::a::PICC picc;
    if (!s.units.begin() || !picc.emulate(m5::nfc::a::Type::NTAG_213, uid.data(), uid.size()) ||
        !s.emulation.begin(picc, s.memory.data(), s.memory.size())) {
        Stop();
        return false;
    }
    s.active = true;
    s.until  = now;
    return true;
}
bool NfcUrl::Stop()
{
    if (!impl_ || !impl_->needs_stop) return true;
    auto& s    = *impl_;
    s.active   = false;
    bool ended = s.emulation.end();
    bool off   = s.unit.writeOperationControl(0);
    if (!ended || !off) return false;
    s.needs_stop = false;
    s.until      = 0;
    s.memory.fill(0);
    return true;
}
bool NfcUrl::Poll(uint64_t now)
{
    if (!impl_ || !impl_->active) return false;
    if (WindowExpired(now, impl_->until, 60000)) {
        Stop();
        return false;
    }
    impl_->units.update();
    impl_->emulation.update();
    return true;
}
bool NfcUrl::Active() const
{
    return impl_ && impl_->active;
}
}  // namespace stock_tools
