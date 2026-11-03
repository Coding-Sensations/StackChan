from pathlib import Path
import subprocess,tempfile,re
r=Path(__file__).resolve().parent
from muse_codec_fixture import production_fixture
with tempfile.TemporaryDirectory(prefix='muse-codec-') as temporary:
 t=Path(temporary);cpp=t/'test.cpp';binary=t/'test'
 cpp.write_text(production_fixture('muse_cold_output_test.cpp'))
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DSTACKCHAN_BUILD_MUSE=1','-DSTACKCHAN_BUILD_MUSE_VOICE_PROTOCOL=1','-I'+str(r),'-I'+str(r/'muse-codec-fixtures'),'-I'+str(r.parent/'main'),str(cpp),'-o',str(binary)],check=True)
 for case in range(71):subprocess.run([str(binary),str(case)],check=True,timeout=10)


print("PASS 71 cold production-body cases with inert SDK ports; no hardware activation or physical-flush proof")
