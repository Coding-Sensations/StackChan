// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_credentials.hpp"
namespace companion::muse {
// Requires an already initialized existing NVS partition and an explicit
// storage grant in the calling session. Never initializes, erases or formats
// NVS, changes encryption/eFuses, or reads any namespace except its own explicitly scoped record.
class DirectNvsStore final : public CredentialStore,public DeviceCredentialReader {
public:
    bool CanCreateNew();
    bool LoadAccess(const std::string& exact_node,std::function<bool()> current,DeviceRecord& output)override;
    StoreResult CommitNew(const DeviceRecord&)override;
};
}
