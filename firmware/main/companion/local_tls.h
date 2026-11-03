// Companion-only TLS transport. Vendor network implementations are unchanged.
#pragma once
#include "tcp.h"
#include "diagnostics.h"
#include <atomic>
#include <mutex>
#include <esp_tls.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
extern "C" const char* companion_local_ca_pem();
extern "C" const char* companion_local_tls_host();
namespace companion {
class LocalTls final : public Tcp {
public:
    explicit LocalTls(diagnostic::Task role=diagnostic::Task::AudioTls);
    ~LocalTls() override;
    bool Connect(const std::string& host,int port) override;
    void Disconnect() override;
    int Send(const std::string& data) override;
    int GetLastError() override;
private:
    esp_tls_t* tls_client_=nullptr;
    EventGroupHandle_t event_group_=nullptr;
    TaskHandle_t receive_task_handle_=nullptr;
    TaskHandle_t owner_task_=nullptr;
    std::atomic<bool> active_{false};
    int last_error_=0;
    std::mutex send_mutex_; // Preserve whole-write ordering, including retries.
    std::mutex context_mutex_; // One nonblocking SDK read/write at a time.
    int socket_fd_=-1; // Established once; owner closes only after receive/writer join.
    diagnostic::Task diagnostic_role_=diagnostic::Task::AudioTls;
    std::atomic<TaskHandle_t> diagnostic_receive_task_{nullptr};
    diagnostic::Task CallerRole() const;
    void ReceiveTask();
};
}
