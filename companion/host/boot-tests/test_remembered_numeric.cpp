#include "numeric-fixture/sdk_boundary.hpp"
#include "numeric_pairing.h"
#include <cassert>
#include <iostream>
const char* fixture_ca="synthetic-CA-no-authority";
extern "C" const char* companion_local_ca_pem(){return fixture_ca;}
extern "C" const char* companion_local_tls_host(){return "192.168.42.10";}
using namespace companion;
PairPolicy policy(){return {PairDevice,PairUri,STACKCHAN_PIN_CA_SHA256,STACKCHAN_PIN_EXPIRY,1900000000};}
std::string context(const NumericPairing& p){return "\"version\":1,\"request_id\":\""+p.Request()+"\",\"device_nonce\":\"0102030405060708090a0b0c0d0e0f10\",\"host_nonce\":\""+std::string(32,'b')+"\"";}
std::string challenge(const NumericPairing& p,bool audio=false){return "{\"type\":\"pair.remember.challenge\","+context(p)+",\"code\":\"123456\",\"expires_ms\":60000,\"audio_requested\":"+(audio?"true":"false")+",\"host_ca_sha256\":\""+STACKCHAN_PIN_CA_SHA256+"\",\"remember_until_unix\":2000000000}";}
std::string grant(const NumericPairing& p){return "{\"type\":\"pair.remember.grant\","+context(p)+",\"device\":\""+PairDevice+"\",\"uri\":\""+PairUri+"\",\"credential\":\"synthetic-only-never-real-secret-123456\",\"ttl_seconds\":120,\"audio_requested\":false,\"host_ca_sha256\":\""+STACKCHAN_PIN_CA_SHA256+"\",\"remember_until_unix\":2000000000}";}
int main(){NetworkInterface network;unsigned saves=0;bool save_ok=true;auto current=policy();
 auto configure=[&](NumericPairing& p){p.ConfigureRemembered([&]{return current;},[&](const PairRecord& record){assert(ValidPairRecord(record,current));++saves;return save_ok;});};
 {NumericPairing p;assert(!p.StartRemembered(network,123,0));}
 {NumericPairing p;configure(p);current.now=current.expiry;assert(!p.StartRemembered(network,123,0));current=policy();}
 {NumericPairing p;configure(p);fixture_ca="changed-synthetic-CA";assert(!p.StartRemembered(network,123,0));fixture_ca="synthetic-CA-no-authority";}
 {NumericPairing p;configure(p);assert(p.StartRemembered(network,123,0));auto ws=BoundedWebSocket::latest;assert(ws->sent[0].find("pair.remember.hello")!=std::string::npos);ws->Feed(challenge(p,true));p.Tick(1);assert(!p.Active(1)&&saves==0);}
 {NumericPairing p;configure(p);assert(p.StartRemembered(network,123,0));auto ws=BoundedWebSocket::latest;ws->Feed(challenge(p));p.Tick(1);ws->Feed(grant(p));p.Tick(2);assert(!p.Active(2)&&saves==0);}
 {NumericPairing p;configure(p);assert(p.StartRemembered(network,123,0));auto ws=BoundedWebSocket::latest;ws->Feed(challenge(p));p.Tick(1);assert(p.Confirm(2));current.ca=std::string(64,'b');ws->Feed(grant(p));p.Tick(3);assert(!p.Active(3)&&saves==0);current=policy();}
 {NumericPairing p;configure(p);assert(p.StartRemembered(network,123,0));auto ws=BoundedWebSocket::latest;ws->Feed(challenge(p));p.Tick(1);assert(p.Confirm(2));assert(ws->sent.back().find("\"remember\":true")!=std::string::npos);ws->Feed(grant(p));p.Tick(3);assert(saves==1&&!p.RememberedReady());NumericGrant out;assert(!p.TakeGrant(out));assert(ws->sent.back().find("pair.remember.stored")!=std::string::npos);ws->Feed("{\"type\":\"pair.remember.ready\","+context(p)+",\"audio_requested\":false,\"ready\":true}");p.Tick(4);assert(p.RememberedReady()&&p.Remembered());p.Cancel();assert(!p.RememberedReady()&&!p.Remembered());}
 {NumericPairing p;configure(p);save_ok=false;assert(p.StartRemembered(network,123,0));auto ws=BoundedWebSocket::latest;ws->Feed(challenge(p));p.Tick(1);assert(p.Confirm(2));ws->Feed(grant(p));p.Tick(3);assert(!p.Active(3)&&saves==2);}
 std::cout<<"PASS restored remembered parser: policy/trust/consent/no-audio/save/ack/reset\n";
}
