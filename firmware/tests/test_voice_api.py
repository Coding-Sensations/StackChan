from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parent;c=r.parent/'components/muse_direct_protocol'
flags=['-g','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(c),'-I'+str(c/'voice')]
with tempfile.TemporaryDirectory(prefix='voice-api-') as temp:
 t=Path(temp)
 for cmd in [
 ['clang','-std=c11',*flags,'-c',str(c/'voice/muse_voice_call_core.c'),'-o',str(t/'core.o')],
 ['clang++','-std=c++17',*flags,str(r/'test_voice_api.cpp'),str(c/'voice/api.cc'),str(t/'core.o'),'-o',str(t/'test')],
 [str(t/'test')]]:
  p=subprocess.run(cmd,capture_output=True,text=True);assert p.returncode==0,p.stdout+p.stderr
print('PASS dormant function-table lifecycle, null/alignment/size rejection and fixed-storage destruction wipe')
