#pragma once
// Serialized control owner; failed phases retain handle and retry state.
namespace companion {
template<class Handle> struct CheckedUnit {
    Handle handle=nullptr;
    bool needs_stop=false, needs_uninitialize=false;
    int stop_status=0, uninitialize_status=0, dispose_status=0;
    template<class Ops> int stop(Ops& ops) noexcept {
        if(!handle || !needs_stop)return 0;
        stop_status=ops.stop(handle);if(stop_status)return stop_status;needs_stop=false;return 0;
    }
    template<class Ops> int close(Ops& ops) noexcept {
        if(!handle)return 0;
        const int stopped=stop(ops);if(stopped)return stopped;
        if(needs_uninitialize){uninitialize_status=ops.uninitialize(handle);if(uninitialize_status)return uninitialize_status;needs_uninitialize=false;}
        dispose_status=ops.dispose(handle);if(dispose_status)return dispose_status;
        handle=nullptr;return 0;
    }
};
}
