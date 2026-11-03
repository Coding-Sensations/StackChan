#include <cassert>
#include <string_view>
#include <string>
#if __has_include("../../../firmware/main/companion/local_tls_policy.hpp")
#include "../../../firmware/main/companion/local_tls_policy.hpp"
#else
// Characterize the old transport: every WSS host uses public bundled roots.
namespace companion { enum class TlsRoute { Public, Local, Reject };
inline TlsRoute SelectTlsRoute(std::string_view, std::string_view, std::string_view) { return TlsRoute::Public; } }
#endif
#ifndef COMPANION_TLS_CHECKED_PORT
namespace companion { inline bool ParseTlsPort(std::string_view text,int& port){port=std::stoi(std::string(text));return true;} }
#endif
struct Config {
    const unsigned char* cacert_buf=nullptr; unsigned cacert_bytes=0;
    const char* common_name=nullptr; bool skip_common_name=true;
    int timeout_ms=0; void* crt_bundle_attach=reinterpret_cast<void*>(1);
};
int main() {
    int port=0;
    assert(companion::ParseTlsPort("443",port) && port==443);
    assert(companion::ParseTlsPort("65535",port) && port==65535);
    for(auto bad : {"bad","","0","65536","999999999999999999999","-1","+443","443extra"," 443"})
        assert(!companion::ParseTlsPort(bad,port));
    Config cfg;
    assert(companion::ConfigureLocalTls(cfg,"CA","192.168.1.12","192.168.1.12",443));
    assert(std::string_view(reinterpret_cast<const char*>(cfg.cacert_buf))=="CA" && cfg.cacert_bytes==3);
    assert(std::string_view(cfg.common_name)=="192.168.1.12" && !cfg.skip_common_name);
    assert(cfg.crt_bundle_attach==nullptr && cfg.timeout_ms==5000);
    assert(!companion::ConfigureLocalTls(cfg,"CA","192.168.1.12","192.168.1.13",443));
    assert(!companion::ConfigureLocalTls(cfg,"CA","192.168.1.12","192.168.1.12",0));
    using companion::SelectTlsRoute; using companion::TlsRoute;
    assert(SelectTlsRoute("", "", "public.example") == TlsRoute::Public);
    assert(SelectTlsRoute("CA", "192.168.1.12", "192.168.1.12") == TlsRoute::Local);
    assert(SelectTlsRoute("CA", "192.168.1.12", "192.168.1.13") == TlsRoute::Reject);
    assert(SelectTlsRoute("CA", "192.168.1.12", "public.example") == TlsRoute::Reject);
    assert(SelectTlsRoute("", "192.168.1.12", "192.168.1.12") == TlsRoute::Reject);
    assert(SelectTlsRoute("CA", "", "192.168.1.12") == TlsRoute::Reject);
    for (auto bad : {"192.168.001.12", "192.168.1.256", "192.168.1.12.evil", "192.168.1.12:443", "192.168.1.12\n", "8.8.8.8", "127.0.0.1", "0.0.0.0", "192.168.1"})
        assert(SelectTlsRoute("CA", bad, bad) == TlsRoute::Reject);
    assert(SelectTlsRoute("CA", "10.1.2.3", "10.1.2.3") == TlsRoute::Local);
    assert(SelectTlsRoute("CA", "172.16.0.2", "172.16.0.2") == TlsRoute::Local);
    assert(SelectTlsRoute("CA", "172.32.0.2", "172.32.0.2") == TlsRoute::Reject);
}
