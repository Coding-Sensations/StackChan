"""Compile actual diagnostic entry/settings and actual HAL body with inert ports."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parent;main=r.parent/'main'
flags=['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DSTACKCHAN_MUSE_LOCAL_DIAGNOSTIC=1']
with tempfile.TemporaryDirectory(prefix='cold-startup-') as tmp:
 t=Path(tmp)
 (t/'main-test.cpp').write_text('''#include <cassert>
namespace companion::muse::local_diag{int calls=0;void RunLocalDiagnostic(){++calls;}}
extern "C" void app_main();
int main(){app_main();assert(companion::muse::local_diag::calls==1);}
''')
 subprocess.run([*flags,'-I'+str(main),str(main/'main.cpp'),str(t/'main-test.cpp'),'-o',str(t/'entry')],check=True);subprocess.run([t/'entry'],check=True)
 # No NVS declaration or symbol exists in this port: any diagnostic storage call
 # causes a compile/link failure. Native checks separately use the actual SDK header.
 (t/'settings.h').write_text('''#pragma once
#include <string>
#include <cstdint>
class Settings{public:std::string ns_;bool read_write_=false;Settings(const std::string&,bool=false);~Settings();std::string GetString(const std::string&,const std::string&="");void SetString(const std::string&,const std::string&);int32_t GetInt(const std::string&,int32_t=0);void SetInt(const std::string&,int32_t);bool GetBool(const std::string&,bool=false);void SetBool(const std::string&,bool);void EraseKey(const std::string&);void EraseAll();};
'''.replace('&="','& = "'))
 (t/'settings-test.cpp').write_text('''#include "settings.h"
#include <cassert>
int main(){for(const char*ns:{"board","audio","wifi","account","muse"}){Settings s(ns,true);s.SetString("synthetic-key","synthetic-only");s.SetInt("value",7);s.SetBool("flag",true);assert(s.GetString("synthetic-key","default")=="default"&&s.GetInt("value",3)==3&&!s.GetBool("flag",false));s.EraseKey("synthetic-key");s.EraseAll();}}
''')
 subprocess.run([*flags,'-I'+str(t),str(main/'companion/readonly_settings.cc'),str(t/'settings-test.cpp'),'-o',str(t/'settings')],check=True);subprocess.run([t/'settings'],check=True)
 source=(main/'hal/hal.cpp').read_text();sig='bool Hal::initMuseDiagnostic()';start=source.index(sig);i=source.index('{',start)+1;depth=1
 while depth:depth+=(source[i]=='{')-(source[i]=='}');i+=1
 body=source[start:i]
 (t/'hal.cpp').write_text('''#include <cassert>
#include <vector>
std::vector<int> calls;
class Hal{public:bool initMuseDiagnostic();void xiaozhi_board_init(){calls.push_back(1);}void io_expander_init(){calls.push_back(2);}void setServoPowerEnabled(bool v){assert(!v);calls.push_back(3);}void setLaserEnabled(bool v){assert(!v);calls.push_back(4);}void lvgl_init(){calls.push_back(5);}};
'''+body+'''
int main(){Hal h;assert(h.initMuseDiagnostic());assert(calls==std::vector<int>({1,2,3,4,5}));}
''')
 subprocess.run([*flags,t/'hal.cpp','-o',t/'hal'],check=True);subprocess.run([t/'hal'],check=True)
print('PASS actual diagnostic app_main/storage-free settings/HAL startup routing; inert ports, no device boot')
