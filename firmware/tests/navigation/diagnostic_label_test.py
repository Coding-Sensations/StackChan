"""Production label/status mapping at an inert LVGL boundary."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[3]
source=(root/'firmware/main/apps/app_setup/workers/network_test.cpp').read_text()
def extract(signature):
    start=source.index(signature);begin=source.index('{',start);depth=1;end=begin+1
    while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
code=r'''
#include "hal/utils/network_diagnostics/network_diagnostics.h"
#include <cassert>
#include <string>
#include <cstdio>
struct Label{std::string text;int color=0;void setText(const std::string& value){text=value;}void setTextColor(int value){color=value;}};
int lv_color_hex(int value){return value;}
'''+extract('std::string description(')+'\n'+extract('void updateResult(')+r'''
int main(){using namespace network_diagnostics;Label label;std::string previous;
 for(const char* service:{"Xiaozhi","M5Stack"}){
  updateResult(label,previous,service,{Status::NotTested,"Not tested: no current Wi-Fi session."});
  assert(label.text.find("Not tested:")!=std::string::npos&&label.text.find("HTTP")==std::string::npos);assert(label.color==0x26206A);
 }
 updateResult(label,previous,"M5Stack",{Status::Failed,"Connection failed",503});assert(label.color==0xB3261E&&label.text.find("HTTP 503")!=std::string::npos);
 updateResult(label,previous,"M5Stack",{Status::Available,"Connected",200});assert(label.color==0x217A43);
 puts("4 actual service-label/status paths passed; inert LVGL only");
}
'''
with tempfile.TemporaryDirectory(prefix='stackchan-service-label-') as temp:
    p=Path(temp);(p/'test.cpp').write_text(code)
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'firmware/main'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
