// Adapted from 78/esp-ml307 under Apache-2.0; local companion-only bounds.
// See WEBSOCKET-VENDOR.json and Apache-2.0.txt. Original package remains unchanged.
#include "bounded_web_socket.h"
#include "local_tls.h"
#include "local_tls_policy.hpp"
#include "local_tls_lifecycle.hpp"
#include "transport_write.hpp"
#include <esp_random.h>
#include <mbedtls/sha1.h>
#include "websocket_handshake.hpp"
#include "network_interface.h"
#include <esp_log.h>
#include <cstdlib>
#include <exception>
#include <cstring>
#include <esp_pthread.h>


#define TAG "BoundedWebSocket"

static std::string base64_encode(const unsigned char *data, size_t len) {
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    size_t i = 0;
    while (i < len) {
        size_t chunk_size = std::min((size_t)3, len - i);

        for (size_t j = 0; j < 3; j++) {
            char_array_3[j] = (j < chunk_size) ? data[i + j] : 0;
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (size_t j = 0; j < 4; j++) {
            if (j <= chunk_size) {
                encoded.push_back(base64_chars[char_array_4[j]]);
            } else {
                encoded.push_back('=');
            }
        }

        i += chunk_size;
    }
    return encoded;
}


BoundedWebSocket::BoundedWebSocket(NetworkInterface* network, int connect_id) : network_(network), connect_id_(connect_id) {
    handshake_event_group_ = xEventGroupCreate();
}

BoundedWebSocket::~BoundedWebSocket() {
    // The owner retains this object across Close. Receive callbacks must not
    // release its last reference. Join before releasing callback event state.
    if(callback_task_.load()==xTaskGetCurrentTaskHandle())std::terminate();
    connected_=false;
    companion::ReleaseStoppedTransport(tcp_,send_mutex_);
    if (handshake_event_group_) {
        vEventGroupDelete(handshake_event_group_);
    }
}

void BoundedWebSocket::SetHeader(const char* key, const char* value) {
    headers_[key] = value;
}

void BoundedWebSocket::SetReceiveBufferSize(size_t size) {
    receive_buffer_size_ = size;
}

bool BoundedWebSocket::IsConnected() const {
    return connected_;
}

bool BoundedWebSocket::Connect(const char* uri) {
    std::string uri_str(uri);
    std::string protocol, host, port, path;
    size_t pos = 0;
    size_t next_pos = 0;

    // 解析协议
    next_pos = uri_str.find("://");
    if (next_pos == std::string::npos) {
        ESP_LOGE(TAG, "Invalid URI format");
        return false;
    }
    protocol = uri_str.substr(0, next_pos);
    const char* local_ca=companion_local_ca_pem();
    const char* local_host=companion_local_tls_host();
    if(((local_ca && *local_ca) || (local_host && *local_host)) && protocol!="wss")return false;
    pos = next_pos + 3;

    // 解析主机
    next_pos = uri_str.find(':', pos);
    if (next_pos == std::string::npos) {
        next_pos = uri_str.find('/', pos);
        if (next_pos == std::string::npos) {
            host = uri_str.substr(pos);
            path = "/";
        } else {
            host = uri_str.substr(pos, next_pos - pos);
            path = uri_str.substr(next_pos);
        }
        port = (protocol == "wss") ? "443" : "80";
    } else {
        host = uri_str.substr(pos, next_pos - pos);
        pos = next_pos + 1;

        // 解析端口
        next_pos = uri_str.find('/', pos);
        if (next_pos == std::string::npos) {
            port = uri_str.substr(pos);
            path = "/";
        } else {
            port = uri_str.substr(pos, next_pos - pos);
            path = uri_str.substr(next_pos);
        }
    }

    int port_number=0;
    if(!companion::ParseTlsPort(port,port_number))return false;

    ESP_LOGD(TAG, "Connecting to %s://%s:%s%s", protocol.c_str(), host.c_str(), port.c_str(), path.c_str());

    // 设置 BoundedWebSocket 特定的头部
    SetHeader("Upgrade", "websocket");
    SetHeader("Connection", "Upgrade");
    SetHeader("Sec-WebSocket-Version", "13");

    // 生成随机的 Sec-WebSocket-Key
    char key[25];
    esp_fill_random(key,16);
    std::string base64_key = base64_encode(reinterpret_cast<const unsigned char*>(key), 16);
    SetHeader("Sec-WebSocket-Key", base64_key.c_str());
    auto accept_input=base64_key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char digest[20];
    if(mbedtls_sha1(reinterpret_cast<const unsigned char*>(accept_input.data()),accept_input.size(),digest)!=0)return false;
    expected_accept_=base64_encode(digest,sizeof(digest));
    handshake_completed_=false;failed_=false;receive_buffer_.clear();current_message_.clear();


    if (protocol == "wss" || protocol == "https") {
        const char* ca=companion_local_ca_pem();
        const char* expected=companion_local_tls_host();
        auto route=companion::SelectTlsRoute(ca?ca:"",expected?expected:"",host);
        if(route==companion::TlsRoute::Reject)return false;
        if(route==companion::TlsRoute::Local)tcp_=std::make_unique<companion::LocalTls>(connect_id_==3?companion::diagnostic::Task::PairTls:companion::diagnostic::Task::AudioTls);
        else tcp_=network_->CreateSsl(connect_id_);
    } else {
        tcp_ = network_->CreateTcp(connect_id_);
    }

    connected_ = false;
    // 清除事件位
    xEventGroupClearBits(handshake_event_group_, HANDSHAKE_SUCCESS_BIT | HANDSHAKE_FAILED_BIT);

    // 设置数据接收回调来处理握手和后续的WebSocket帧
    tcp_->OnStream([this](const std::string& data) {
        callback_task_=xTaskGetCurrentTaskHandle();
        this->OnTcpData(data);
    });

    // 设置断开连接回调
    tcp_->OnDisconnected([this]() {
        callback_task_=xTaskGetCurrentTaskHandle();
        if (connected_) {
            connected_ = false;
            if (on_disconnected_) {
                on_disconnected_();
            }
        }
    });

    // 使用 tcp 建立连接
    if (!tcp_->Connect(host, port_number)) {
        ESP_LOGE(TAG, "Failed to connect to server");
        return false;
    }

    // 发送 BoundedWebSocket 握手请求
    std::string request = "GET " + path + " HTTP/1.1\r\n";
    if (headers_.find("Host") == headers_.end()) {
        request += "Host: " + host + "\r\n";
    }
    for (const auto& header : headers_) {
        request += header.first + ": " + header.second + "\r\n";
    }
    request += "\r\n";

    if (companion::CompleteTransportWrite(tcp_->Send(request),request.size()) < 0) {
        ESP_LOGE(TAG, "Failed to send BoundedWebSocket handshake request");
        return false;
    }

    // 等待握手完成，超时时间10秒
    EventBits_t bits = xEventGroupWaitBits(
        handshake_event_group_,
        HANDSHAKE_SUCCESS_BIT | HANDSHAKE_FAILED_BIT,
        pdFALSE,  // 不清除事件位
        pdFALSE,  // 等待任意一个事件位
        pdMS_TO_TICKS(10000)  // 10秒超时
    );

    if (bits & HANDSHAKE_SUCCESS_BIT) {
        connected_ = true;
        if (on_connected_) {
            on_connected_();
        }
        return true;
    } else if (bits & HANDSHAKE_FAILED_BIT) {
        ESP_LOGE(TAG, "BoundedWebSocket handshake failed");
        if (on_error_) {
            on_error_(-1);
        }
        return false;
    } else {
        ESP_LOGE(TAG, "BoundedWebSocket handshake timeout");
        return false;
    }
}


bool BoundedWebSocket::Send(const std::string& data) {
    return Send(data.data(), data.size(), false);
}

bool BoundedWebSocket::Send(const void* data, size_t len, bool binary, bool fin) {
    if (len > 4112) {
        ESP_LOGE(TAG, "Data too large, maximum supported size is 65535 bytes");
        return false;
    }

    return companion::WithLiveTransport(tcp_,connected_,send_mutex_,[&](Tcp& transport){
        std::string frame;
        frame.reserve(len + 8);  // 最大可能的帧大小（2字节帧头 + 2字节长度 + 4字节mask）

        // 第一个字节：FIN 位 + 操作码
        uint8_t first_byte = (fin ? 0x80 : 0x00);
        if (binary) {
            first_byte |= 0x02;  // 二进制帧
        } else if (!continuation_) {
            first_byte |= 0x01;  // 文本帧
        } // 否则，操作码为0（延续帧）

        frame.push_back(static_cast<char>(first_byte));

        // 第二个字节：MASK 位 + 有效载荷长度
        if (len < 126) {
            frame.push_back(static_cast<char>(0x80 | len));  // 设置MASK位
        } else {
            frame.push_back(static_cast<char>(0x80 | 126));  // 设置MASK位
            frame.push_back(static_cast<char>((len >> 8) & 0xFF));
            frame.push_back(static_cast<char>(len & 0xFF));
        }

        // 生成随机的4字节mask
        uint8_t mask[4];
        esp_fill_random(mask,4);
        frame.append(reinterpret_cast<const char*>(mask), 4);

        // 添加并mask处理有效载荷
        const uint8_t* payload = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < len; ++i) {
            frame.push_back(static_cast<char>(payload[i] ^ mask[i % 4]));
        }

        // 更新continuation_状态
        continuation_ = !fin;

        // 发送帧
        return companion::CompleteTransportWrite(transport.Send(frame),frame.size());
    }) > 0;
}

void BoundedWebSocket::Ping() {
    SendControlFrame(0x9, nullptr, 0);
}

void BoundedWebSocket::Close() {
    // A callback can request closure, but the retaining owner must join later.
    if(callback_task_.load()==xTaskGetCurrentTaskHandle()){connected_=false;return;}
    if (connected_) {
        SendControlFrame(0x8, nullptr, 0);
    }
    connected_=false;
    companion::StopTransport(tcp_.get());
    std::lock_guard<std::mutex> writers(send_mutex_);
    callback_task_=nullptr;
}

void BoundedWebSocket::OnConnected(std::function<void()> callback) {
    on_connected_ = callback;
}

void BoundedWebSocket::OnDisconnected(std::function<void()> callback) {
    on_disconnected_ = callback;
}

void BoundedWebSocket::OnData(std::function<void(const char*, size_t, bool binary)> callback) {
    on_data_ = callback;
}

void BoundedWebSocket::OnError(std::function<void(int)> callback) {
    on_error_ = callback;
}

int BoundedWebSocket::GetLastError() {
    if (tcp_) {
        return tcp_->GetLastError();
    }
    return 0;  // No error if TCP connection doesn't exist
}

void BoundedWebSocket::OnTcpData(const std::string& data) {
    // 将新数据追加到接收缓冲区
    if (failed_) return;
    if (data.size()>16384 || receive_buffer_.size()>16384-data.size()) { Fail(); return; }
    receive_buffer_.append(data);

    if (!handshake_completed_) {
        // 检查握手响应
        size_t pos = receive_buffer_.find("\r\n\r\n");
        if (pos != std::string::npos) {
            std::string handshake_response = receive_buffer_.substr(0, pos + 4);
            receive_buffer_ = receive_buffer_.substr(pos + 4);

            if (companion::valid_upgrade(handshake_response,expected_accept_)) {
                handshake_completed_ = true;
                // 设置握手成功事件
                xEventGroupSetBits(handshake_event_group_, HANDSHAKE_SUCCESS_BIT);
            } else {
                ESP_LOGE(TAG, "BoundedWebSocket handshake failed");
                // 设置握手失败事件
                xEventGroupSetBits(handshake_event_group_, HANDSHAKE_FAILED_BIT);
                return;
            }
        } else {
            // 握手响应未完整接收
            return;
        }
    }

    // 处理WebSocket帧
    size_t buffer_offset = 0;
    const uint8_t* buffer = reinterpret_cast<const uint8_t*>(receive_buffer_.data());
    size_t buffer_size = receive_buffer_.size();

    while (buffer_offset < buffer_size) {
        if (buffer_size - buffer_offset < 2) break; // 需要更多数据

        uint8_t opcode = buffer[buffer_offset] & 0x0F;
        bool fin = (buffer[buffer_offset] & 0x80) != 0;
        uint8_t mask = buffer[buffer_offset + 1] & 0x80;
        uint64_t payload_length = buffer[buffer_offset + 1] & 0x7F;

        size_t header_length = 2;
        if (payload_length == 126) {
            if (buffer_size - buffer_offset < 4) break; // 需要更多数据
            payload_length = (static_cast<uint64_t>(buffer[buffer_offset + 2]) << 8) | buffer[buffer_offset + 3];
            header_length += 2;
        } else if (payload_length == 127) {
            if (buffer_size - buffer_offset < 10) break; // 需要更多数据
            payload_length = 0;
            for (int i = 0; i < 8; ++i) {
                payload_length = (payload_length << 8) | buffer[buffer_offset + 2 + i];
            }
            header_length += 8;
        }

        if (payload_length>4112 || mask || (buffer[buffer_offset]&0x70) || (opcode>=8 && (!fin || payload_length>125))) { Fail(); return; }
        uint8_t mask_key[4] = {0};
        if (mask) {
            if (buffer_size - buffer_offset < header_length + 4) break; // 需要更多数据
            memcpy(mask_key, buffer + buffer_offset + header_length, 4);
            header_length += 4;
        }

        if (buffer_size - buffer_offset < header_length + payload_length) break; // 需要更多数据

        // 解码有效载荷
        std::vector<char> payload(payload_length);
        if (payload_length > 0) {
            memcpy(payload.data(), buffer + buffer_offset + header_length, payload_length);
            if (mask) {
                for (size_t i = 0; i < payload_length; ++i) {
                    payload[i] ^= mask_key[i % 4];
                }
            }
        }

        // 处理帧
        switch (opcode) {
            case 0x0: // 延续帧
            case 0x1: // 文本帧
            case 0x2: // 二进制帧
                if (opcode != 0x0 && is_fragmented_) {
                    ESP_LOGE(TAG, "Received new message frame while still fragmenting");
                    Fail(); return;
                }
                if (opcode != 0x0) {
                    is_fragmented_ = !fin;
                    is_binary_ = (opcode == 0x2);
                    current_message_.clear();
                }
                if ((opcode==0 && !is_fragmented_) || current_message_.size()+payload.size()>4112) { Fail(); return; }
                current_message_.insert(current_message_.end(), payload.begin(), payload.end());
                if (fin) {
                    if (on_data_) {
                        on_data_(current_message_.data(), current_message_.size(), is_binary_);
                    }
                    current_message_.clear();
                    is_fragmented_ = false;
                }
                break;
            case 0x8: // 关闭帧
                connected_ = false;
                if (on_disconnected_) {
                    on_disconnected_();
                }
                break;
            case 0x9: // Ping
                SendControlFrame(0xA, payload.data(), payload_length);
                break;
            case 0xA: // Pong
                break;
            default:
                ESP_LOGE(TAG, "Unknown opcode: %d", opcode);
                break;
        }

        buffer_offset += header_length + payload_length;
    }

    // 保留未处理的数据
    if (buffer_offset > 0) {
        receive_buffer_ = receive_buffer_.substr(buffer_offset);
    }
}



bool BoundedWebSocket::SendControlFrame(uint8_t opcode, const void* data, size_t len) {
    if (len > 125) {
        ESP_LOGE(TAG, "控制帧有效载荷过大");
        return false;
    }

    std::string frame;
    frame.reserve(len + 6);  // 帧头 + 掩码 + 有效载荷

    // 第一个字节：FIN 位 + 操作码
    frame.push_back(static_cast<char>(0x80 | opcode));

    // 第二个字节：MASK 位 + 有效载荷长度
    frame.push_back(static_cast<char>(0x80 | len));

    // 生成随机的4字节掩码
    uint8_t mask[4];
    esp_fill_random(mask,4);
    frame.append(reinterpret_cast<const char*>(mask), 4);

    // 添加并掩码处理有效载荷
    const uint8_t* payload = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        frame.push_back(static_cast<char>(payload[i] ^ mask[i % 4]));
    }

    // 发送帧
    return companion::WithLiveTransport(tcp_,connected_,send_mutex_,[&](Tcp& transport){return companion::CompleteTransportWrite(transport.Send(frame),frame.size());}) > 0;
}
void BoundedWebSocket::Fail() {
    failed_=true; connected_=false; receive_buffer_.clear(); current_message_.clear();
    xEventGroupSetBits(handshake_event_group_,HANDSHAKE_FAILED_BIT);
    if(on_error_)on_error_(-1);
}
