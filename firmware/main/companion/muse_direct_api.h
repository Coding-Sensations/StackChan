// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_credentials.hpp"
namespace companion::muse {
class DirectApiVerifier final : public DeviceVerifier,public ExactVmLookup {
    std::function<bool()> current_;
    cJSON* Fetch(const DeviceRecord&);
public:
    explicit DirectApiVerifier(std::function<bool()> current):current_(std::move(current)){}
    bool Verify(const DeviceRecord&)override;
    bool FindExactVm(const DeviceRecord&,const std::string& exact_vm,VmCredential& output)override;
};
}
