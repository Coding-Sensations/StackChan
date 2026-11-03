#!/usr/bin/env python3
"""Portable actual diagnostic core/bridge tests, SDK clock/stack/RTC doubles only."""
import pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[3]
def main():
 with tempfile.TemporaryDirectory(prefix='stackchan-diagnostics-offline-') as folder:
  tmp=pathlib.Path(folder);(tmp/'freertos').mkdir()
  (tmp/'esp_attr.h').write_text('#pragma once\n#define RTC_NOINIT_ATTR\n#define DRAM_ATTR\n')
  (tmp/'esp_timer.h').write_text('#pragma once\n#include <cstdint>\ninline int64_t esp_timer_get_time(){return 100000;}\n')
  (tmp/'freertos/FreeRTOS.h').write_text('#pragma once\n')
  (tmp/'freertos/task.h').write_text('#pragma once\n#include <cassert>\ninline unsigned uxTaskGetStackHighWaterMark(void* task){assert(task==nullptr);return 1024;}\n')
  (tmp/'bridge.cpp').write_text('#include <companion/diagnostic_state.hpp>\n#include <cassert>\nusing namespace companion::diagnostic;\nint main(){Init(123,15);assert(BootNonce()==123);Stage(Task::Main,StageCode::Boot);Sample(Task::Main);Fault(Task::Codec,FaultCode::DecodeError,-7);Fault(Task::Main,FaultCode::Write,-1);CountTick();CountHeartbeat(false);auto f=Snapshot();assert(f.size==176&&Read32(f.bytes.data()+20)==1&&Read32(f.bytes.data()+28)==1&&Read32(f.bytes.data()+36)==1&&Read32(f.bytes.data()+40)==4&&Read32(f.bytes.data()+44)==uint32_t(-7)&&Read32(f.bytes.data()+60)==1024&&Read32(f.bytes.data()+172)==Crc32(f.bytes.data(),172));}\n')
  flags=['clang++','-std=c++17','-Wall','-Wextra','-Werror','-pthread','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(ROOT/'firmware/main')]
  core=tmp/'core';subprocess.run(flags+[str(pathlib.Path(__file__).with_name('test_diagnostic_state.cpp')),'-o',str(core)],check=True);subprocess.run([str(core)],check=True,timeout=10)
  bridge=tmp/'bridge';subprocess.run(flags+['-I'+str(tmp),str(tmp/'bridge.cpp'),str(ROOT/'firmware/main/companion/diagnostics.cc'),'-o',str(bridge)],check=True);subprocess.run([str(bridge)],check=True,timeout=10)
 print('PASS actual atomic core + actual diagnostics.cc; SDK/RTC doubles; no hardware/network')
if __name__=='__main__':main()
