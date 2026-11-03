#include "numeric-fixture/sdk_boundary.hpp"
#include "numeric_pairing.h"
#include <cassert>
#include <iostream>
extern "C" const char* companion_local_ca_pem(){return "synthetic-CA-no-authority";}
extern "C" const char* companion_local_tls_host(){return "192.168.42.10";}
const std::string r(32,'a'),d="0102030405060708090a0b0c0d0e0f10",h(32,'b');
std::string challenge(){return "{\"type\":\"pair.challenge\",\"version\":1,\"request_id\":\""+r+"\",\"device_nonce\":\""+d+"\",\"host_nonce\":\""+h+"\",\"code\":\"123456\",\"expires_ms\":60000,\"audio_requested\":true}";}
std::string grant(){return "{\"type\":\"pair.grant\",\"version\":1,\"request_id\":\""+r+"\",\"device_nonce\":\""+d+"\",\"host_nonce\":\""+h+"\",\"device\":\"stackchan-001122334455\",\"uri\":\"wss://192.168.42.10:18443/companion/audio\",\"credential\":\"synthetic-only-never-real-secret-123456\",\"ttl_seconds\":120,\"audio_requested\":true}";}
std::string ready(){return "{\"type\":\"pair.ready\",\"version\":1,\"request_id\":\""+r+"\",\"device_nonce\":\""+d+"\",\"host_nonce\":\""+h+"\",\"audio_requested\":true,\"ready\":true}";}
int main(){using namespace companion;NetworkInterface network;
 {NumericPairing p;assert(p.Start(network,r,123,0));auto ws=BoundedWebSocket::latest;assert(ws->sent.size()==1);ws->Feed(challenge());p.Tick(1);assert(p.Code()=="123456"&&p.AudioRequested());assert(p.Confirm(2));assert(p.Code()=="123456"&&p.Confirmed());assert(ws->sent.size()==2);assert(p.Confirm(2)&&ws->sent.size()==2);ws->Feed(grant());p.Tick(3);NumericGrant out;assert(p.TakeGrant(out));assert(out.expires_at_ms==120000&&out.audio_requested);assert(p.Staged()&&ws->sent.size()==3);assert(!p.ConnectionReady());ws->Feed(ready());p.Tick(4);assert(p.ConnectionReady());assert(ws->live);p.Tick(500);assert(ws->live&&p.ConnectionReady()&&p.Active(500));assert(p.Active(119999));p.Tick(120000);assert(!p.Active(120000)&&!p.ConnectionReady());}
 {NumericPairing p;assert(p.Start(network,r,123,0));BoundedWebSocket::latest->Feed(grant());p.Tick(1);NumericGrant out;assert(!p.TakeGrant(out)&&!p.Active(1));}
 for(auto bad:std::vector<std::string>{ready(),"{}","{\"type\":\"pair.challenge\",\"version\":1,\"version\":1}",challenge()+"garbage", "{\"nested\":{\"value\":1}}", "{\"a\":[1]}"}){NumericPairing p;assert(p.Start(network,r,123,0));BoundedWebSocket::latest->Feed(bad);p.Tick(1);assert(!p.Active(1));}
 {NumericPairing p;assert(p.Start(network,r,123,0));auto ws=BoundedWebSocket::latest;ws->Feed(challenge());p.Tick(1);assert(p.Confirm(2));ws->disconnected();p.Tick(3);assert(!p.Active(3));}
 for(auto replacement:std::vector<std::string>{"false","1","\"true\""}){NumericPairing p;assert(p.Start(network,r,123,0));auto ws=BoundedWebSocket::latest;ws->Feed(challenge());p.Tick(1);assert(p.Confirm(2));auto bad=grant();auto pos=bad.find("\"audio_requested\":true");assert(pos!=std::string::npos);bad.replace(pos,std::string("\"audio_requested\":true").size(),"\"audio_requested\":"+replacement);ws->Feed(bad);p.Tick(3);NumericGrant out;assert(!p.TakeGrant(out)&&!p.Active(3));}
 std::cout<<"PASS actual numeric TLS owner/parser with in-memory transport boundary\n";
}
