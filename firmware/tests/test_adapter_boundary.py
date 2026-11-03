from pathlib import Path
import subprocess
r=Path(__file__).resolve().parent
fw=r.parent
import tempfile
workspace=tempfile.TemporaryDirectory(prefix='adapter-primary-')
products=Path(workspace.name)
p=subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(fw/'main'),str(r/'test_adapter_boundary.cpp'),'-o',str(products/'adapter-boundary-test')],capture_output=True,text=True);(products/'adapter-compile.log').write_text(p.stdout+p.stderr)
if p.returncode:print(p.stderr[-1200:]);raise SystemExit(p.returncode)
p=subprocess.run([str(products/'adapter-boundary-test')],capture_output=True,text=True);(products/'adapter-run.log').write_text(p.stdout+p.stderr);print('adapter boundary exit',p.returncode)
if p.returncode:raise SystemExit(p.returncode)

import tempfile
with tempfile.TemporaryDirectory(prefix='adapter-profile-') as temp:
 t=Path(temp);cpp=t/'parked.cpp';cpp.write_text('#include "companion/companion_adapter_contract.hpp"\n#include <cassert>\nint main(){using P=companion_modes::Provider;for(auto p:{P::Hermes,P::Muse,P::Claude,P::Codex}){auto d=companion::adapter::Describe(p);assert((d.kind==companion::adapter::Kind::NotBuilt)==!bool(companion_modes::Find(p)));if(!companion_modes::Find(p))assert(!d.private_setup&&!d.text_route_present&&!d.voice_route_present&&!d.a2a_available);}}')
 for parked in [['HERMES','MUSE','CLAUDE','CODEX'],['HERMES'],['MUSE'],['CLAUDE'],['CODEX']]:
  a=['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(fw/'main')]+['-DSTACKCHAN_BUILD_'+p+'=0' for p in parked]+[str(cpp),'-o',str(t/'test')]
  subprocess.run(a,check=True);subprocess.run([str(t/'test')],check=True)
print('PASS adapter static capabilities across parked-provider profiles; no authority or live route claimed')
