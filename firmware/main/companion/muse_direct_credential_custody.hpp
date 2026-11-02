// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_direct_credential_policy.hpp"
#include "muse_direct_credentials.hpp"
#if !STACKCHAN_MUSE_RAM_ONLY
#include "muse_direct_store.h"
#endif
namespace companion::muse {
// Called only at provider entry/exit, never from a credential callback.
void DeviceCredentialsProviderVisible(bool visible);
// One lease per retained worker Window. Acquire never renews an old lease.
// Scoped callbacks must be bounded and may call Current, but not lifecycle/store APIs.
class CredentialLease final:public CredentialStore,public DeviceCredentialReader {
#if STACKCHAN_MUSE_RAM_ONLY
 uint32_t epoch_=0;
#else
 DirectNvsStore store_;bool acquired_=false;
#endif
public:
 CredentialLease()=default;CredentialLease(const CredentialLease&)=delete;CredentialLease&operator=(const CredentialLease&)=delete;
 CredentialLease(CredentialLease&&)=delete;CredentialLease&operator=(CredentialLease&&)=delete;
 bool Acquire();bool Current()const;bool CanCreateNew();
 StoreResult CommitNew(const DeviceRecord&)override;
 bool LoadAccess(const std::string&,std::function<bool()>,DeviceRecord&)override;
};
}
