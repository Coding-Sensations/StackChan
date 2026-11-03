#include <apps/stock_shell/shell_teardown.hpp>
#include <cassert>
#include <functional>
#include <memory>
#include <vector>
namespace {
int lock_depth=0,closed=0,opened=0,globals_cleared=0;
std::function<void()> home_callback;
std::vector<bool> destruction_locked;
struct UiLock{UiLock(){++lock_depth;}~UiLock(){--lock_depth;}};
struct SetupOwner:mooncake::AppAbility{
 int marker=42;
 ~SetupOwner()override{destruction_locked.push_back(lock_depth>0);}
 void onOpen()override{++opened;home_callback=[this]{assert(marker==42);};}
 void onClose()override{assert(lock_depth>0);++closed;home_callback={};}
};
int start(mooncake::Mooncake& shell){
 auto id=shell.installApp(std::make_unique<SetupOwner>());shell.update();assert(shell.openApp(id));shell.update();assert(home_callback);return id;
}
void clear_globals(){assert(lock_depth>0);home_callback={};++globals_cleared;}
}
int main(){
 // Actual vendor behavior reproduces the defect: uninstall omits close and
 // leaves the global [this] callback pointing at a destroyed UI owner.
 {mooncake::Mooncake old;start(old);old.uninstallAllApps();assert(closed==0&&home_callback);assert(!destruction_locked.back());home_callback={};}
 closed=opened=globals_cleared=0;destruction_locked.clear();
 {
  auto shell=std::make_unique<mooncake::Mooncake>();start(*shell);
  stock_shell::TearDownShell<UiLock>(*shell,clear_globals,[&]{assert(lock_depth>0);shell.reset();});
  assert(!shell&&!home_callback&&closed==1&&globals_cleared==1);assert(destruction_locked.size()==1&&destruction_locked[0]);assert(lock_depth==0);
 }
 // Pending open must be cancelled, not run during teardown.
 {
  auto shell=std::make_unique<mooncake::Mooncake>();auto id=shell->installApp(std::make_unique<SetupOwner>());shell->update();shell->openApp(id);auto before=opened;
  stock_shell::TearDownShell<UiLock>(*shell,clear_globals,[&]{shell.reset();});assert(opened==before&&!home_callback&&lock_depth==0);assert(destruction_locked.back());
 }
 // Already sleeping owners are still destroyed under the lock, and globals
 // are cleared even when no live close callback can detach them.
 {
  auto shell=std::make_unique<mooncake::Mooncake>();shell->installApp(std::make_unique<SetupOwner>());shell->update();home_callback=[]{};
  auto before=closed;stock_shell::TearDownShell<UiLock>(*shell,clear_globals,[&]{shell.reset();});assert(closed==before&&!home_callback&&destruction_locked.back());
 }
}
