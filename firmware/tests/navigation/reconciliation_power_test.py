"""Execute real board power polling and timer routing without PMIC/device IO."""
from pathlib import Path
import shutil
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[2] / 'main/hal/board/stackchan.cc').read_text()


def extract(signature):
    start = source.index(signature)
    position = source.index('{', start)
    depth, end = 1, position + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end].replace(' override', '')


code = r'''
#include <cassert>
#include <cstdint>
#include <mutex>
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_ERROR_CHECK(value) assert((value)==0)
#define STACKCHAN_COMPANION_BOOT 1
using esp_err_t=int;constexpr int ESP_OK=0,ESP_TIMER_TASK=0;
int64_t now=5000;int64_t esp_timer_get_time(){return now*1000;}
namespace companion {enum class AudioOwner{None,Native,Stock};AudioOwner owner=AudioOwner::None;
 AudioOwner CurrentAudioOwner(){return owner;}}
struct esp_timer_create_args_t {void(*callback)(void*);void* arg;int dispatch_method;const char* name;bool skip_unhandled_events;};
esp_timer_create_args_t timer;int esp_timer_create(esp_timer_create_args_t* value,void** out){timer=*value;*out=(void*)1;return 0;}
int esp_timer_start_periodic(void*,int){return 0;}
struct Pmic {int level=75,error=0;bool external=true,charging=true,discharging=false;
 int TryReadPowerStatus(bool& a,bool& b,bool& c){a=external;b=charging;c=discharging;return error;}
 int TryReadBatteryLevel(int& value){value=level;return error;}};
struct PowerTimer {bool enabled=false;int updates=0;void SetEnabled(bool value){enabled=value;++updates;}};
struct M5StackCoreS3Board {Pmic storage,*pmic_=&storage;PowerTimer power,*power_save_timer_=nullptr;
 struct {bool allowShutdownWhenCharging=true;}xiaozhi_config_;
 struct {bool has_external_power=false,charging=false,discharging=false,status_valid=false,battery_level_valid=false;int battery_level=0;}power_state_;
 std::mutex power_state_mutex_;bool last_power_save_enabled_=false;int consecutive_power_read_failures_=0;
 int64_t last_power_error_log_ms_=0,last_power_state_check_ms_=0;
 static constexpr int kPowerReadErrorLogIntervalMs=30000,kPowerStatePollIntervalMs=1000;
 void* touchpad_timer_=nullptr;int initializations=0;
 void PollTouchpad(){}void InitializePowerSaveTimer(){power_save_timer_=&power;++initializations;}
''' + '\n'.join(extract(signature) for signature in [
    'bool ShouldEnablePowerSave(', 'void UpdatePowerSaveEnabled(', 'void RefreshPowerState()',
    'void PollPowerSaveState()', 'void StartTouchpadTimer()', 'virtual bool GetBatteryLevel(']) + r'''
};
int main(){
 M5StackCoreS3Board board;board.StartTouchpadTimer();int level=0;bool charging=false,discharging=true;
 timer.callback(timer.arg);assert(board.GetBatteryLevel(level,charging,discharging));
 assert(level==75&&charging&&!discharging&&!board.initializations);
 companion::owner=companion::AudioOwner::Native;now+=5000;board.storage.level=31;timer.callback(timer.arg);
 assert(board.GetBatteryLevel(level,charging,discharging)&&level==31&&!board.initializations);
 companion::owner=companion::AudioOwner::Stock;now+=5000;timer.callback(timer.arg);
 assert(board.initializations==1&&board.power.enabled);
 companion::owner=companion::AudioOwner::None;now+=5000;board.storage.level=22;timer.callback(timer.arg);
 assert(!board.power.enabled&&board.GetBatteryLevel(level,charging,discharging)&&level==22);
 board.storage.error=-1;board.storage.level=99;now+=5000;timer.callback(timer.arg);
 assert(board.GetBatteryLevel(level,charging,discharging)&&level==22&&!board.power.enabled);
}
'''
with tempfile.TemporaryDirectory(prefix='stackchan-power-reconciliation-') as temporary:
    directory = Path(temporary)
    cpp, exe = directory / 'power.cpp', directory / 'power'
    cpp.write_text(code)
    subprocess.run([shutil.which('clang++') or 'c++', '-std=c++17', '-fsanitize=address,undefined',
                    str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Home/Native battery stays fresh, failed reads retain cache, and only Stock enables shutdown timer')
