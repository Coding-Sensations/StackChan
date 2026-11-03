from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='muse-pcm-') as tmp:
 binary=Path(tmp)/'test'
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-DSTACKCHAN_BUILD_MUSE=1','-DSTACKCHAN_BUILD_MUSE_VOICE_PROTOCOL=1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(r/'muse-pcm-fixtures'),'-I'+str(r.parent/'main'),str(r/'muse_pcm_test.cpp'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True,timeout=10)
