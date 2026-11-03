from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parent;v=r.parent/'components/muse_direct_protocol/voice'
with tempfile.TemporaryDirectory(prefix='voice-bridge-') as temp:
 t=Path(temp);flags=['-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(v),'-I'+str(v.parent),'-I'+str(r.parent/'main/companion')]
 for cmd in [['clang','-std=c11',*flags,'-c',str(v/'muse_voice_call_core.c'),'-o',str(t/'core.o')],['clang++','-std=c++17',*flags,str(r/'voice_handoff_test.cpp'),str(t/'core.o'),'-o',str(t/'test')],[str(t/'test')]]:
  p=subprocess.run(cmd,capture_output=True,text=True);assert p.returncode==0,p.stdout+p.stderr
print('PASS all four review direction masks -> retained actual codec owner; no native permit, zero codec/common audio calls')
