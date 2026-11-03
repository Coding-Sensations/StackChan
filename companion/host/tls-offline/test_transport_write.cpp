#include <cassert>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include "../../../firmware/main/companion/local_tls_lifecycle.hpp"
#include "../../../firmware/main/companion/transport_write.hpp"
struct Transport { int result; int Send(const std::string&) { return result; } };
int main() {
    for(int result : {0, 1, 47, -1, -7, 48}) {
        auto transport=std::make_unique<Transport>();transport->result=result;
        std::atomic<bool> live{true};std::mutex writers;
        bool interrupted=false;
        const std::string heartbeat(48,'x');
        auto sent=companion::WithLiveTransport(transport,live,writers,[&](Transport& t){
            return companion::CompleteTransportWrite(t.Send(heartbeat),heartbeat.size());
        });
        bool success=sent>0;
        if(!success)interrupted=true;
        assert(success==(result==48));
        assert(interrupted==(result!=48));
        assert(companion::CompleteTransportWrite(result,48)==(result==48?48:result<0?result:-1));
    }
    assert(companion::CompleteTransportWrite(49,48)<0);
    assert(companion::CompleteTransportWrite(0,0)<0);
}
