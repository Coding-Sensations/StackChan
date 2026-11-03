#include "numeric-fixture/sdk_boundary.hpp"
#include "numeric_pairing.h"
#include <cassert>
#include <iostream>
extern "C" const char* companion_local_ca_pem(){return "synthetic-CA-no-authority";}
extern "C" const char* companion_local_tls_host(){return "192.168.42.10";}
int main(){using namespace companion;NetworkInterface network;NumericPairing p;
 assert(p.StartWireless(network,91,100));
 auto ws=BoundedWebSocket::latest;assert(ws&&ws->sent.size()==1);
 const auto request=p.Request();assert(PairHex(request));
 assert(ws->sent[0].find("\"request_id\":\""+request+"\"")!=std::string::npos);
 assert(ws->sent[0].find("\"boot_nonce\":91")!=std::string::npos);
 std::string d="0102030405060708090a0b0c0d0e0f10",h(32,'b');
 auto context="\"version\":1,\"request_id\":\""+request+"\",\"device_nonce\":\""+d+"\",\"host_nonce\":\""+h+"\"";
 ws->Feed("{\"type\":\"pair.challenge\","+context+",\"code\":\"123456\",\"expires_ms\":60000,\"audio_requested\":true}");p.Tick(101);
 assert(p.Code()=="123456"&&!p.Confirmed());NumericGrant out;assert(!p.TakeGrant(out));
 assert(p.Confirm(102));
 ws->Feed("{\"type\":\"pair.grant\","+context+",\"device\":\"stackchan-001122334455\",\"uri\":\"wss://192.168.42.10:18443/companion/audio\",\"credential\":\"synthetic-only-00000000000000000000\",\"ttl_seconds\":60,\"audio_requested\":true}");p.Tick(103);
 assert(p.TakeGrant(out)&&out.request==request&&out.audio_requested);assert(p.Staged());assert(!p.ConnectionReady());
 ws->Feed("{\"type\":\"pair.ready\","+context+",\"audio_requested\":true,\"ready\":true}");p.Tick(104);assert(p.ConnectionReady());
 p.Tick(60103);assert(!p.Active(60103));
 assert(!p.StartWireless(network,0,100));
 std::cout<<"PASS actual wireless boot/hello/physical-match/grant/ready/deadline without USB\n";
}
