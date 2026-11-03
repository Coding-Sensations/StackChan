// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_native_socket.h"
#include "muse_direct_credentials.hpp"
namespace companion::muse {
class NativeVmLookup final:public ExactVmLookup {
 std::function<bool()> current_;NativeMetaStream stream_;bool spent_=false;
 uint64_t end_=0,last_=0;bool revoked_=false;
 bool Current();bool Write(std::string&);bool Read(uint8_t*,size_t);bool Line(std::string&,size_t);
public:
 explicit NativeVmLookup(std::function<bool()> current):current_(current),stream_(std::move(current),OfficialEndpoint::Api){}
 bool FindExactVm(const DeviceRecord&,const std::string&,VmCredential&)override;
 bool CloseAndJoin(unsigned timeout){return stream_.CloseAndJoin(timeout);}
};
}
