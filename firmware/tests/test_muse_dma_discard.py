from pathlib import Path
import subprocess,tempfile
from muse_codec_fixture import production_fixture
r=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='muse-discard-') as temporary:
 t=Path(temporary);cpp=t/'test.cpp';cpp.write_text(production_fixture('muse_dma_discard_test.cpp'))
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DSTACKCHAN_BUILD_MUSE=1','-DSTACKCHAN_BUILD_MUSE_VOICE_PROTOCOL=1','-I'+str(r/'muse-codec-fixtures'),'-I'+str(r.parent/'main'),str(cpp),'-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test')],check=True,timeout=10)
