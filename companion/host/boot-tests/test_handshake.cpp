#include <cassert>
#include <string>
#include "websocket_handshake.hpp"
int main(){
std::string accept="s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
std::string good="HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: keep-alive, Upgrade\r\nSec-WebSocket-Accept: "+accept+"\r\n\r\n";
assert(companion::valid_upgrade(good,accept));
assert(!companion::valid_upgrade(good,"wrong"));
assert(!companion::valid_upgrade("HTTP/1.1 200 OK\r\n\r\n",accept));
assert(!companion::valid_upgrade(good.substr(0,good.size()-2),accept));
auto dup=good;dup.insert(dup.size()-2,"sec-websocket-accept: "+accept+"\r\n");assert(!companion::valid_upgrade(dup,accept));
auto ext=good;ext.insert(ext.size()-2,"Sec-WebSocket-Extensions: permessage-deflate\r\n");assert(!companion::valid_upgrade(ext,accept));
auto bad=good;bad.replace(bad.find("keep-alive, Upgrade"),19,"notupgrade");assert(!companion::valid_upgrade(bad,accept));
assert(!companion::valid_upgrade(std::string(8193,'a'),accept));
}
