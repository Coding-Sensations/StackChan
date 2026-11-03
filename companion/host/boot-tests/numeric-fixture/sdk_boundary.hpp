#pragma once
#define COMPANION_BOUNDED_WEBSOCKET_H
#include <string>
#include <vector>
#include <functional>
#include <cstddef>
#include "companion/diagnostics.h"
namespace companion::diagnostic {inline void Stage(Task,StageCode){}inline void Sample(Task){}}
class NetworkInterface{};
class BoundedWebSocket {
public:
 static inline BoundedWebSocket* latest=nullptr;bool live=false;std::vector<std::string> sent;
 std::function<void(const char*,size_t,bool)> data;std::function<void()> disconnected;std::function<void(int)> error;
 BoundedWebSocket(NetworkInterface*,int){latest=this;}
 void SetReceiveBufferSize(size_t){}void OnData(decltype(data)f){data=f;}void OnDisconnected(decltype(disconnected)f){disconnected=f;}void OnError(decltype(error)f){error=f;}
 bool Connect(const char*){live=true;return true;}bool IsConnected()const{return live;}bool Send(const std::string&s){sent.push_back(s);return live;}void Close(){live=false;}
 void Feed(const std::string&s){data(s.data(),s.size(),false);}
};
