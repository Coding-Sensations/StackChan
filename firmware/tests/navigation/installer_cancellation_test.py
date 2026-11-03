"""Execute generated OTA production code against inert HTTP/flash boundaries."""
from pathlib import Path
import importlib.util
import shutil
import subprocess
import tempfile

firmware = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('backend', firmware / 'main/apps/stock_agent/generate_backend.py')
backend = importlib.util.module_from_spec(spec)
spec.loader.exec_module(backend)


def extract(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth, end = 1, body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


code = r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
using esp_err_t=int;using esp_ota_handle_t=int;
constexpr int ESP_OK=0,ESP_ERR_OTA_VALIDATE_FAILED=-2,OTA_WITH_SEQUENTIAL_WRITES=0,MALLOC_CAP_INTERNAL=0;
struct esp_image_header_t{char bytes[24];};struct esp_image_segment_header_t{char bytes[8];};
struct esp_app_desc_t{char version[32];};
struct esp_partition_t{const char* label="test";uint32_t address=0,size=65536;}partition;
enum Mode{Success,CancelRead,CancelCallback,CancelWrite,CancelClose,CancelValidation,CancelCommit,RevokeValidation,WriteFailure,CancelPrepare,Oversize};
Mode mode=Success;bool home=false,permission=true,active_handle=false;std::function<bool()> pending_boot;int opens=0,reads=0,writes=0,aborts=0,ends=0,selections=0,allocations=0;
int64_t clock_us=0;int64_t esp_timer_get_time(){return clock_us+=1000000;}
void* heap_caps_malloc(size_t size,int){++allocations;return malloc(size);}
void heap_caps_free(void* ptr){assert(allocations>0);--allocations;free(ptr);}
const esp_partition_t* esp_ota_get_next_update_partition(void*){return &partition;}
int esp_ota_begin(const esp_partition_t*,int,int* handle){active_handle=true;*handle=17;return 0;}
int esp_ota_abort(int handle){assert(handle==17&&active_handle);active_handle=false;++aborts;return 0;}
int esp_ota_write(int handle,const void*,size_t){assert(handle==17&&active_handle);++writes;if(mode==CancelWrite)home=true;return mode==WriteFailure?-1:0;}
int esp_ota_end(int handle){assert(handle==17&&active_handle);active_handle=false;++ends;if(mode==CancelValidation)home=true;if(mode==RevokeValidation)permission=false;return 0;}
int esp_ota_set_boot_partition(const esp_partition_t*){++selections;assert(!home);return 0;}
namespace runtime_lifecycle {
bool HomeRequested(){return home;}
bool StageFirmwareInstall(const std::function<bool()>& select){if(mode==CancelCommit)home=true;if(home)return false;pending_boot=select;return true;}
bool CommitFirmwareInstall(const std::function<bool()>& reboot){if(home){pending_boot={};return false;}auto select=std::move(pending_boot);pending_boot={};return (!select||select())&&reboot();}
}
namespace stock_services {
bool FirmwareInstallAllowed(){return permission&&!home;}
bool IsFirmwareImageUrl(const std::string&){return true;}
bool PrepareOriginalFirmwareWifi(){if(mode==CancelPrepare)home=true;return true;}
}
struct Http {
 int remaining=8192;void SetTimeout(int){}bool Open(const char*,const std::string&){++opens;return true;}
 int GetStatusCode(){return 200;}size_t GetBodyLength(){return mode==Oversize?70000:8192;}
 int Read(char* buffer,size_t capacity){++reads;if(mode==CancelRead)home=true;int amount=remaining<static_cast<int>(capacity)?remaining:capacity;memset(buffer,0,amount);remaining-=amount;return amount;}
 void Close(){if(mode==CancelClose)home=true;}
};
struct Network{std::unique_ptr<Http>CreateHttp(int){return std::make_unique<Http>();}};
struct Board{Network network;static Board& GetInstance(){static Board b;return b;}Network* GetNetwork(){return &network;}};
struct Ota{static bool Upgrade(const std::string&,std::function<void(int,size_t)>);};
'''
cases = r'''
void reset(Mode next){assert(!active_handle&&allocations==0);pending_boot={};mode=next;home=false;permission=true;opens=reads=writes=aborts=ends=selections=0;clock_us=0;}
bool run(){return Ota::Upgrade("https://example.invalid/test.bin",[](int,size_t){if(mode==CancelCallback)home=true;});}
int main(){
 // Returning from Upgrade must not persist boot selection before caller delays.
 reset(Success);assert(run());assert(writes==2&&ends==1&&selections==0&&!active_handle&&!allocations);
 // Scope permission expires before NativeInstall's final reboot; staging must
 // retain only the validated partition, never a permission/stack reference.
 permission=false;assert(runtime_lifecycle::CommitFirmwareInstall([]{return true;}));assert(selections==1);
 reset(Success);assert(run());home=true;assert(!runtime_lifecycle::CommitFirmwareInstall([]{assert(false);return true;}));assert(selections==0);
 for(auto next:{CancelRead,CancelCallback,CancelWrite,CancelClose,CancelValidation,CancelCommit,CancelPrepare,RevokeValidation}){
  reset(next);assert(!run());assert(selections==0&&!active_handle&&!allocations);
  if(next==CancelRead||next==CancelCallback)assert(writes==0);
  if(next==CancelWrite)assert(writes==1&&aborts==1);
  if(next==CancelClose)assert(ends==0&&aborts==1);
  if(next==CancelValidation||next==RevokeValidation)assert(ends==1&&aborts==0);
  if(next==CancelPrepare)assert(opens==0);
 }
 reset(WriteFailure);assert(!run()&&writes==1&&aborts==1&&!allocations&&!selections);
 reset(Oversize);assert(!run()&&!writes&&!allocations&&!selections);
 reset(Success);permission=false;assert(!run()&&!opens&&!writes);
}
'''
with tempfile.TemporaryDirectory(prefix='stackchan-installer-cancel-') as temporary:
    directory = Path(temporary)
    backend.generate(firmware / 'xiaozhi-esp32/main', firmware / 'managed_components/78__esp-wifi-connect', directory)
    production = (directory / 'stock_ota.cc').read_text()
    cpp, exe = directory / 'ota.cpp', directory / 'ota'
    cpp.write_text(code + extract(production, 'bool Ota::Upgrade(') + cases)
    subprocess.run([shutil.which('clang++') or 'c++', '-std=c++17', '-fsanitize=address,undefined',
                    str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
print('Production OTA cancellation: read/callback/write/EOF/validation/commit/preparation and success/error paths passed')
