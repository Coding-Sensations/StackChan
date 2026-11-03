// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ble_owner.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
struct ble_gap_event;
struct ble_gatt_access_ctxt;
namespace companion::muse {
// Explicit owner-thread API. Construction performs no IO. All callbacks are
// bounded notifications into a retained owner's queue, never SDK/NVS work.
class DirectBle {
    BleLease lease_;
    std::function<bool()> current_;
    std::function<void(const uint8_t*,size_t)> receive_;
    std::function<void()> disconnected_;
    std::string name_;
    std::atomic<bool> closing_{false},exited_{true},started_{false},subscribed_{false},clean_{true};
    std::atomic<uint16_t> connection_{0xffff},tx_handle_{0};
    TaskHandle_t task_=nullptr;
    std::atomic<TaskHandle_t> owner_{nullptr};
    bool initialized_=false;
    uint8_t address_type_=0;
    std::mutex tx_mutex_;
    bool Current()const;
    void Advertise();
public:
    DirectBle()=default;
    DirectBle(const DirectBle&)=delete;
    bool Start(const std::string& name,std::function<bool()> current,
        std::function<void(const uint8_t*,size_t)> receive,std::function<void()> disconnected);
    bool Send(const std::string& message);
    // Foreign/callback stop revokes only. Retaining owner must join externally.
    bool StopAndJoin();
    bool CleanupComplete()const{return clean_;}
    // Static C callbacks dispatch only to the retained singleton stack owner.
    static int Gap(struct ble_gap_event*,void*);
    static int Access(uint16_t,uint16_t,struct ble_gatt_access_ctxt*,void*);
    static void Sync();
    static void Reset(int);
    static void Host(void*);
};
}
