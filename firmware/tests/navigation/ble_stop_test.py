"""Exercise production BLE host parking and external stop order without BLE IO."""
import pathlib,subprocess,tempfile,shutil
source=(pathlib.Path(__file__).resolve().parents[2]/'main/hal/utils/bleprph/bleprph.c').read_text()
def function(signature):
 start=source.index(signature);p=source.index('{',start);d=1;e=p+1
 while d:d+=(source[e]=='{')-(source[e]=='}');e+=1
 return source[start:e]
fixture=r'''
#include <atomic>
#include <cassert>
#include <cstddef>
#define atomic_load std::atomic_load
#define atomic_store std::atomic_store
#define ESP_LOGI(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
constexpr int ESP_OK=0;
std::atomic<bool> home_ble_initialized{false},home_ble_host_started{false},home_ble_host_exited{true};
bool sdk_handle=false,host_parked=false,external_deleted=false,stack_deinitialized=false,callbacks_revoked=false;
int nimble_port_stop(){assert(sdk_handle);return 0;}
void nimble_port_run(){assert(sdk_handle);}
void vTaskSuspend(void*){host_parked=true;throw 1;}
void vTaskDelay(int){assert(false&&"host parking receipt should already exist");}
void nimble_port_freertos_deinit(){assert(sdk_handle&&host_parked&&home_ble_host_exited);sdk_handle=false;external_deleted=true;}
int nimble_port_deinit(){assert(external_deleted&&!sdk_handle);stack_deinitialized=true;return 0;}
struct stackchan_ble_callbacks_t{int callback=0;};
void stackchan_ble_register_callbacks(const stackchan_ble_callbacks_t* c){assert(stack_deinitialized&&c->callback==0);callbacks_revoked=true;}
'''+function('void bleprph_host_task(void *param)')+'\n'+function('bool ble_prph_stop_checked(void)')+r'''
int main(){
 assert(ble_prph_stop_checked());
 for(int repeat=0;repeat<50;++repeat){
  sdk_handle=true;host_parked=external_deleted=stack_deinitialized=callbacks_revoked=false;
  home_ble_initialized=true;home_ble_host_started=true;home_ble_host_exited=false;
  try{bleprph_host_task(nullptr);}catch(int){}
  assert(sdk_handle&&host_parked&&!external_deleted); // live handle until external owner deletes it
  assert(ble_prph_stop_checked());assert(!sdk_handle&&callbacks_revoked&&!home_ble_initialized);
  assert(ble_prph_stop_checked());
 }
}
'''
with tempfile.TemporaryDirectory(prefix='stackchan-ble-stop-') as tmp:
 tmp=pathlib.Path(tmp);cpp=tmp/'ble.cpp';cpp.write_text(fixture);exe=tmp/'ble'
 subprocess.run([shutil.which('clang++') or 'c++','-std=c++17','-fsanitize=address,undefined',str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 print('50 repeated BLE stops park host before external SDK task deletion, deinit and callback revocation')
