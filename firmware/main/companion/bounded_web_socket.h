// Adapted from 78/esp-ml307 under Apache-2.0; local companion-only bounds.
// See WEBSOCKET-VENDOR.json and Apache-2.0.txt. Original package remains unchanged.
#ifndef COMPANION_BOUNDED_WEBSOCKET_H
#define COMPANION_BOUNDED_WEBSOCKET_H

#include <functional>
#include <atomic>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "tcp.h"

class NetworkInterface;

class BoundedWebSocket {
public:
    BoundedWebSocket(NetworkInterface* network, int connect_id);
    ~BoundedWebSocket();

    void SetHeader(const char* key, const char* value);
    void SetReceiveBufferSize(size_t size);
    bool IsConnected() const;
    bool Connect(const char* uri);
    int HandshakeStatus()const{return handshake_status_.load();}
    bool Send(const std::string& data);
    bool Send(const void* data, size_t len, bool binary = false, bool fin = true);
    void Ping();
    void Close();

    void OnConnected(std::function<void()> callback);
    void OnDisconnected(std::function<void()> callback);
    void OnData(std::function<void(const char*, size_t, bool binary)> callback);
    void OnError(std::function<void(int)> callback);

    // 获取最后一次错误码
    int GetLastError();

private:
    std::atomic<int> handshake_status_{0};
    NetworkInterface* network_;
    int connect_id_;
    std::unique_ptr<Tcp> tcp_;
    std::atomic<TaskHandle_t> callback_task_{nullptr};
    bool continuation_ = false; // Accessed only inside the writer lock.
    size_t receive_buffer_size_ = 2048;
    std::string receive_buffer_;
    std::string expected_accept_;
    bool handshake_completed_ = false;
    std::atomic<bool> connected_{false};
    bool failed_ = false;
    void Fail();

    // Mutex for sending data and replying pong
    std::mutex send_mutex_;
    
    EventGroupHandle_t handshake_event_group_;
    static const EventBits_t HANDSHAKE_SUCCESS_BIT = BIT0;
    static const EventBits_t HANDSHAKE_FAILED_BIT = BIT1;

    std::map<std::string, std::string> headers_;
    std::function<void(const char*, size_t, bool binary)> on_data_;
    std::function<void(int)> on_error_;
    std::function<void()> on_connected_;
    std::function<void()> on_disconnected_;

    std::vector<char> current_message_;
    bool is_fragmented_ = false;
    bool is_binary_ = false;

    void OnTcpData(const std::string& data);
    bool SendControlFrame(uint8_t opcode, const void* data, size_t len);
};

#endif // COMPANION_BOUNDED_WEBSOCKET_H
