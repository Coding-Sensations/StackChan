from pathlib import Path
import subprocess,tempfile
from muse_codec_fixture import production_fixture
r=Path(__file__).resolve().parent
CASES=[0,1,2,3,4,5,6,*range(10,24),30,31,32,33]
with tempfile.TemporaryDirectory(prefix='muse-amp-reset-') as temporary:
 t=Path(temporary);cpp=t/'test.cpp';binary=t/'test'
 cpp.write_text(production_fixture('muse_amp_reset_test.cpp'))
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DSTACKCHAN_BUILD_MUSE=1','-DSTACKCHAN_BUILD_MUSE_VOICE_PROTOCOL=1','-I'+str(r),'-I'+str(r/'muse-codec-fixtures'),'-I'+str(r.parent/'main'),str(cpp),'-o',str(binary)],check=True)
 # Failing cases retain custody by design, so each runs in its own process.
 for case in CASES:subprocess.run([str(binary),str(case)],check=True,timeout=10)
