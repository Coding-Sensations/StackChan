#include <cassert>
#include <memory>
#if __has_include("../../../firmware/main/companion/local_tls_lifecycle.hpp")
#include "../../../firmware/main/companion/local_tls_lifecycle.hpp"
#else
namespace companion {
template<class T> void ReleaseStoppedTransport(std::unique_ptr<T>& t) { t.reset(); }
inline bool WriteMayContinue(bool active,long long started,long long now) { (void)active;(void)started;(void)now;return true; }
}
#endif
struct Transport {
    bool& joined; bool& destroyed;
    void Disconnect() { joined=true; }
    ~Transport() { assert(joined); destroyed=true; }
};
int main() {
    // Peer closure cleared the connected flag, but transport still owns a task.
    bool joined=false,destroyed=false;
    auto transport=std::unique_ptr<Transport>(new Transport{joined,destroyed});
    companion::ReleaseStoppedTransport(transport);
    assert(joined && destroyed && !transport);
    assert(companion::WriteMayContinue(true,100,4999999));
    assert(!companion::WriteMayContinue(true,100,5000100));
    assert(!companion::WriteMayContinue(false,100,101));
    // Progress must not reset the total write budget.
    assert(!companion::WriteMayContinue(true,100,6000100));
}
