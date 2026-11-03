// Adapted from 78/esp-ml307 esp_ssl.cc, Apache-2.0. See LOCAL-TLS-VENDOR.json.
#include "local_tls.h"
#include "local_tls_policy.hpp"
#include "local_tls_lifecycle.hpp"
#include "transport_write.hpp"
#include <esp_timer.h>
#include <climits>
#include <fcntl.h>
#include <exception>
#include <mbedtls/build_info.h>
#include <ctime>
#include <sys/socket.h>
#include <esp_log.h>

#include <cstring>
#include <unistd.h>

namespace companion {
static const char *TAG = "CompanionLocalTls";

LocalTls::LocalTls(diagnostic::Task role):diagnostic_role_(role) {
    owner_task_ = xTaskGetCurrentTaskHandle();
    event_group_ = xEventGroupCreate();
}

LocalTls::~LocalTls() {
    // Callbacks may request stop, but must never release the transport owner.
    if(receive_task_handle_ && xTaskGetCurrentTaskHandle()==receive_task_handle_)std::terminate();
    Disconnect();

    if (event_group_ != nullptr) {
        vEventGroupDelete(event_group_);
        event_group_ = nullptr;
    }
}

bool LocalTls::Connect(const std::string& host, int port) {
    configASSERT(xTaskGetCurrentTaskHandle()==owner_task_);
    if(!event_group_)return false;
    const char* ca=companion_local_ca_pem(); const char* expected=companion_local_tls_host();
    if(!ca || !expected || SelectTlsRoute(ca,expected,host)!=TlsRoute::Local || port<1 || port>65535)return false;
#ifndef MBEDTLS_HAVE_TIME_DATE
    return false; // Local profile requires actual certificate expiry checks.
#endif
    if(std::time(nullptr)<1735689600)return false; // Never verify against an unset clock.

    if (tls_client_ != nullptr) {
        ESP_LOGE(TAG, "tls client has been initialized");
        return false;
    }

    tls_client_ = esp_tls_init();
    if (tls_client_ == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize TLS");
        return false;
    }

    esp_tls_cfg_t cfg = {};
    if(!ConfigureLocalTls(cfg,ca,expected,host,port)){esp_tls_conn_destroy(tls_client_);tls_client_=nullptr;return false;}
    // Explicit CA only: no public bundle or global trust fallback.

    int ret = esp_tls_conn_new_sync(host.c_str(), host.length(), port, &cfg, tls_client_);
    if (ret != 1) {
        esp_tls_error_handle_t last_error;
        if (esp_tls_get_error_handle(tls_client_, &last_error) == ESP_OK) {
            int error_code, error_flags;
            esp_err_t err = esp_tls_get_and_clear_last_error(last_error, &error_code, &error_flags);
            last_error_ = err;
            ESP_LOGE(TAG, "Failed to connect to %s:%d, code=0x%x", host.c_str(), port, err);
        } else {
            last_error_ = -1;
            ESP_LOGE(TAG, "Failed to get error handle");
        }
        esp_tls_conn_destroy(tls_client_);
        tls_client_ = nullptr;
        return false;
    }

    // Finish the synchronous handshake first. Thereafter a context lock must
    // never span a socket wait: idle reads must release it for heartbeat writes.
    int fd=-1;
    if(esp_tls_get_conn_sockfd(tls_client_,&fd)!=ESP_OK || fd<0){
        last_error_=-1;esp_tls_conn_destroy(tls_client_);tls_client_=nullptr;return false;
    }
    const int flags=fcntl(fd,F_GETFL,0);
    if(flags<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){
        last_error_=-1;esp_tls_conn_destroy(tls_client_);tls_client_=nullptr;return false;
    }
    const int configured=fcntl(fd,F_GETFL,0);
    if(configured<0 || !(configured&O_NONBLOCK)){
        last_error_=-1;esp_tls_conn_destroy(tls_client_);tls_client_=nullptr;return false;
    }
    socket_fd_=fd;
    active_=true;
    connected_ = true;

    xEventGroupClearBits(event_group_, BIT0);
    if(xTaskCreate([](void* arg) {
        LocalTls* ssl = (LocalTls*)arg;
        ssl->ReceiveTask();
        xEventGroupSetBits(ssl->event_group_, BIT0);
        vTaskDelete(NULL);
    }, "companion_tls", 4096, this, 1, &receive_task_handle_)!=pdPASS) {
        active_=false;connected_=false;esp_tls_conn_destroy(tls_client_);tls_client_=nullptr;socket_fd_=-1;return false;
    }
    return true;
}

void LocalTls::Disconnect() {
    active_=false;
    if(!tls_client_)return;
    if(socket_fd_>=0)shutdown(socket_fd_,SHUT_RDWR);
    // A receive callback can request stop only. Its task, TLS and event group
    // remain alive until the retaining owner calls Disconnect and joins it.
    if(xTaskGetCurrentTaskHandle()==receive_task_handle_)return;
    connected_=false; // Tcp's plain bool is written exclusively by the owner.
    if(receive_task_handle_)
        xEventGroupWaitBits(event_group_,BIT0,pdFALSE,pdFALSE,portMAX_DELAY);
    receive_task_handle_=nullptr;
    // Join receive first, then drain writers (including a receive-side pong).
    std::lock_guard<std::mutex> writers(send_mutex_);
    esp_tls_conn_destroy(tls_client_);tls_client_=nullptr;socket_fd_=-1;
}

/* CONFIG_MBEDTLS_SSL_RENEGOTIATION should be disabled in sdkconfig.
 * Otherwise, invalid memory access may be triggered.
 */
diagnostic::Task LocalTls::CallerRole() const {
    auto caller=xTaskGetCurrentTaskHandle();
    if(caller==diagnostic_receive_task_.load())return diagnostic_role_;
    return caller==owner_task_?diagnostic::Task::Main:diagnostic::Task::Codec;
}
int LocalTls::Send(const std::string& data) {
    const auto diagnostic_task=CallerRole();
    diagnostic::Stage(diagnostic_task,diagnostic::StageCode::TlsWriteEnter);
    // Trace includes writer-lock wait; high-water samples use this caller only.
    struct TraceExit {diagnostic::Task task;~TraceExit(){diagnostic::Stage(task,diagnostic::StageCode::TlsWriteExit);}} trace_exit{diagnostic_task};
    std::lock_guard<std::mutex> writers(send_mutex_);
    if (!active_.load()) {
        ESP_LOGE(TAG, "Not connected");
        diagnostic::Fault(diagnostic_task,diagnostic::FaultCode::TlsWrite,-1);
        return -1;
    }

    if(data.size()>INT_MAX)return -1;
    // Five-second retry budget; a blocking SDK call can overrun this budget.
    // cfg.timeout_ms additionally configures the SDK socket I/O timeout.
    const auto started_us=esp_timer_get_time();
    size_t total_sent = 0;
    size_t data_size = data.size();
    const char* data_ptr = data.data();
    
    while (total_sent < data_size) {
        if(!WriteMayContinue(active_.load(),started_us,esp_timer_get_time())){diagnostic::Fault(diagnostic_task,diagnostic::FaultCode::TlsWrite,-2);return -1;}
        int ret;
        {
            std::lock_guard<std::mutex> context(context_mutex_);
            if(!active_.load())return -1;
            ret=esp_tls_conn_write(tls_client_,data_ptr+total_sent,data_size-total_sent);
        }

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE || ret == ESP_TLS_ERR_SSL_WANT_READ) {
            vTaskDelay(1);
            continue;
        }

        if (ret <= 0) {
            ESP_LOGE(TAG, "SSL send failed: ret=%d, errno=%d", ret, errno);
            diagnostic::Fault(diagnostic_task,diagnostic::FaultCode::TlsWrite,ret);
            return CompleteTransportWrite(ret,data_size-total_sent);
        }
        
        total_sent += ret;
    }
    
    return total_sent;
}

void LocalTls::ReceiveTask() {
    diagnostic_receive_task_.store(xTaskGetCurrentTaskHandle());
    diagnostic::Sample(diagnostic_role_);
    std::string data;
    while (active_.load()) {
        data.resize(1500);
        diagnostic::Stage(diagnostic_role_,diagnostic::StageCode::TlsReadEnter);
        int ret;
        {
            std::lock_guard<std::mutex> context(context_mutex_);
            if(!active_.load())break;
            ret=esp_tls_conn_read(tls_client_,data.data(),data.size());
        } // Release before callbacks: dispatch may synchronously send a pong.
        diagnostic::Stage(diagnostic_role_,diagnostic::StageCode::TlsReadReturn);

        if (ret == ESP_TLS_ERR_SSL_WANT_READ || ret == ESP_TLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(1); // Yield on an idle nonblocking socket, outside the lock.
            continue;
        }

        if (ret <= 0) {
            if (ret < 0) {
                ESP_LOGE(TAG, "SSL receive failed: %d", ret);
            }
            if(active_.load())diagnostic::Fault(diagnostic_role_,diagnostic::FaultCode::TlsRead,ret);
            bool notify=active_.exchange(false);
            if (notify && disconnect_callback_) {
                disconnect_callback_();
            }
            break;
        }
        
        if (active_.load() && stream_callback_) {
            data.resize(ret);
            diagnostic::Stage(diagnostic_role_,diagnostic::StageCode::TlsDispatch);
            stream_callback_(data);
        }
    }
    diagnostic::Sample(diagnostic_role_);
    diagnostic_receive_task_.store(nullptr);
}

int LocalTls::GetLastError() {
    return last_error_;
}

} // namespace companion
