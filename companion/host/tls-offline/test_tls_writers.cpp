#include <atomic>
#include <cassert>
#include <mutex>
#include "../../../firmware/main/companion/local_tls_lifecycle.hpp"
#ifndef COMPANION_TLS_WRITER_QUIESCENCE
namespace companion {
template<class T,class M,class F> int WithLiveTransport(const std::unique_ptr<T>& t,const std::atomic<bool>&,M& m,F f){std::lock_guard<M> guard(m);return f(*t);}
template<class T,class M> void ReleaseStoppedTransport(std::unique_ptr<T>& t,M&){ReleaseStoppedTransport(t);}
}
#endif
struct Mutex {
    bool& joined; bool held=false;
    void lock(){assert(joined);held=true;}
    void unlock(){held=false;}
};
struct Transport {
    bool& joined; Mutex& mutex;
    void Disconnect(){joined=true;}
    ~Transport(){assert(joined && mutex.held);}
};
int main(){
    std::mutex writer;
    std::atomic<bool> live{false};auto transport=std::make_unique<int>(1);bool called=false;
    auto result=companion::WithLiveTransport(transport,live,writer,[&](int&){called=true;return 1;});
    // A writer queued before closure must recheck the live flag under its lock.
    assert(result<0 && !called);
    live=true;
    assert(companion::WithLiveTransport(transport,live,writer,[](int& value){return value;})==1);
    bool prepared=false, probe_joined=true;Mutex probe{probe_joined};
    assert(companion::WithLiveTransport(transport,live,probe,[&](int& value){
        assert(probe.held);prepared=true;return value;
    })==1 && prepared && !probe.held);
    bool joined=false;Mutex mutex{joined};
    std::unique_ptr<Transport> t(new Transport{joined,mutex});
    // Join must precede writer locking: receiver ping may need the same mutex.
    companion::ReleaseStoppedTransport(t,mutex);
    assert(!t && joined && !mutex.held);
}
