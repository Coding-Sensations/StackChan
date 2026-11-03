// SPDX-License-Identifier: Apache-2.0
#include "muse_direct_credential_custody.hpp"
#if STACKCHAN_MUSE_RAM_ONLY
#include <atomic>
#include <mutex>
#include <limits>
namespace companion::muse {
namespace {
struct Custody {
 std::mutex lifecycle,store;
 std::atomic<uint32_t> active{0};uint32_t generation=0;
 Secret<3072> access;Secret<128> node;
};
Custody vault;
}
void DeviceCredentialsProviderVisible(bool visible){
 std::lock_guard<std::mutex> lifecycle(vault.lifecycle);
 if(visible){
  if(vault.active.load())return;
  if(vault.generation==std::numeric_limits<uint32_t>::max())return;
  vault.active.store(++vault.generation);return;
 }
 // Invalidate before waiting for an in-flight copy. Reentry waits for wiping.
 vault.active.store(0);
 std::lock_guard<std::mutex> lock(vault.store);vault.access.Clear();vault.node.Clear();
}
bool CredentialLease::Acquire(){if(epoch_)return false;epoch_=vault.active.load();return epoch_!=0;}
bool CredentialLease::Current()const{return epoch_&&vault.active.load()==epoch_;}
bool CredentialLease::CanCreateNew(){
 std::lock_guard<std::mutex> lock(vault.store);return Current()&&!vault.node.Size()&&!vault.access.Size();
}
StoreResult CredentialLease::CommitNew(const DeviceRecord& input){
 std::lock_guard<std::mutex> lock(vault.store);
 if(!Current()||vault.node.Size()||!Id(input.node)||!input.access.Size()||!input.refresh.Size())return StoreResult::NoWrite;
 if(!vault.access.Set(std::string_view(input.access.Get(),input.access.Size()))||!vault.node.Set(input.node)){vault.access.Clear();vault.node.Clear();return StoreResult::NoWrite;}
 if(!Current()){vault.access.Clear();vault.node.Clear();return StoreResult::NoWrite;}
 return StoreResult::Committed;
}
bool CredentialLease::LoadAccess(const std::string& node,std::function<bool()> current,DeviceRecord& output){
 output.Clear();if(!Current()||!Id(node)||!current||!current())return false;
 std::lock_guard<std::mutex> lock(vault.store);
 if(!Current()||!current()||node!=vault.node.Get()||!vault.access.Size())return false;
 if(!output.access.Set(std::string_view(vault.access.Get(),vault.access.Size())))return false;
 output.node=node;
 if(!current()||!Current()){output.Clear();return false;}return true;
}
}
#else
namespace companion::muse {
void DeviceCredentialsProviderVisible(bool){}
bool CredentialLease::Acquire(){if(acquired_)return false;acquired_=true;return true;}
bool CredentialLease::Current()const{return acquired_;}
bool CredentialLease::CanCreateNew(){return Current()&&store_.CanCreateNew();}
StoreResult CredentialLease::CommitNew(const DeviceRecord& input){return Current()?store_.CommitNew(input):StoreResult::NoWrite;}
bool CredentialLease::LoadAccess(const std::string& node,std::function<bool()> current,DeviceRecord& output){
 if(!Current()){output.Clear();return false;}return store_.LoadAccess(node,std::move(current),output);
}
}
#endif
