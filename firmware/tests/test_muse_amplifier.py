from pathlib import Path
import subprocess,tempfile,re
r=Path(__file__).resolve().parent
from muse_codec_fixture import production_fixture
with tempfile.TemporaryDirectory(prefix='muse-codec-') as temporary:
 t=Path(temporary);cpp=t/'test.cpp';binary=t/'test'
 cpp.write_text(production_fixture('muse_amplifier_test.cpp'))
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DSTACKCHAN_BUILD_MUSE=1','-DSTACKCHAN_BUILD_MUSE_VOICE_PROTOCOL=1','-I'+str(r),'-I'+str(r/'muse-codec-fixtures'),'-I'+str(r.parent/'main'),str(cpp),'-o',str(binary)],check=True)
 for case in range(57):subprocess.run([str(binary),str(case)],check=True,timeout=10)

# Compile the actual class header with both parked closures; no unavailable
# hardware fixture can silently make the parked Muse API available.
with tempfile.TemporaryDirectory(prefix='muse-codec-closure-') as temporary:
 t=Path(temporary);cpp=t/'closure.cpp'
 for muse,voice in [(0,0),(1,0),(1,1)]:
  cpp.write_text('#include <type_traits>\n#include "hal/board/cores3_audio_codec.h"\n'
   'template<class T,class=void>struct HasMuse:std::false_type{};\n'
   'template<class T>struct HasMuse<T,std::void_t<decltype(&T::MuseCodecAcquire)>>:std::true_type{};\n'
   'template<class T,class=void>struct HasQuiesce:std::false_type{};\n'
   'template<class T>struct HasQuiesce<T,std::void_t<decltype(&T::MuseQuiesceForRetirementChecked)>>:std::true_type{};\n'
   f'static_assert(HasQuiesce<CoreS3AudioCodec>::value == {int(bool(muse and voice))});\n'
   'template<class T,class=void>struct HasDiscard:std::false_type{};\n'
   'template<class T>struct HasDiscard<T,std::void_t<decltype(&T::MuseDiscardOutputChecked)>>:std::true_type{};\n'
   f'static_assert(HasDiscard<CoreS3AudioCodec>::value == {int(bool(muse and voice))});\n'
   f'static_assert(HasMuse<CoreS3AudioCodec>::value == {int(bool(muse and voice))});\n' )
  subprocess.run(['clang++','-std=c++17','-fsyntax-only','-Wall','-Wextra','-Werror',f'-DSTACKCHAN_BUILD_MUSE={muse}',f'-DSTACKCHAN_BUILD_MUSE_VOICE_PROTOCOL={voice}','-I'+str(r),'-I'+str(r/'muse-codec-fixtures'),'-I'+str(r.parent/'main'),str(cpp)],check=True)
print('PASS 57 production-body fault cases and Muse/voice-on/off API closure; inert I2C/I2S only')
