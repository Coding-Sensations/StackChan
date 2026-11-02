from pathlib import Path
import subprocess,tempfile,re
r=Path(__file__).resolve().parent
source=(r.parent/'main/hal/board/cores3_audio_codec.cc').read_text()
def body(signature):
 start=source.index(signature);opening=source.index('{',start);depth=0
 for i in range(opening,len(source)):
  depth+=(source[i]=='{')-(source[i]=='}')
  if depth==0:return source[start:i+1]
 raise AssertionError('unclosed production method')
methods=[]
for match in re.finditer(r'(?m)^[\w:]+(?:\s+IRAM_ATTR)?\s+CoreS3AudioCodec::(\w+)\(',source):
 if match.group(1)=='CreateDuplexChannels':continue
 methods.append(body(match.group(0)))
assert any('MuseCodecAcquire' in method for method in methods)
methods='\n'.join(methods)
with tempfile.TemporaryDirectory(prefix='muse-codec-') as temporary:
 t=Path(temporary);cpp=t/'test.cpp';binary=t/'test'
 cpp.write_text((r/'muse_codec_test.cpp').read_text().replace('ACTUAL_METHODS',methods))
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DSTACKCHAN_BUILD_MUSE=1','-DSTACKCHAN_BUILD_MUSE_VOICE_PROTOCOL=1','-I'+str(r/'muse-codec-fixtures'),'-I'+str(r.parent/'main'),str(cpp),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True,timeout=10)

# Compile the actual class header with both parked closures; no unavailable
# hardware fixture can silently make the parked Muse API available.
with tempfile.TemporaryDirectory(prefix='muse-codec-closure-') as temporary:
 t=Path(temporary);cpp=t/'closure.cpp'
 for muse,voice in [(0,0),(1,0),(1,1)]:
  cpp.write_text('#include <type_traits>\n#include "hal/board/cores3_audio_codec.h"\n'
   'template<class T,class=void>struct HasMuse:std::false_type{};\n'
   'template<class T>struct HasMuse<T,std::void_t<decltype(&T::MuseCodecAcquire)>>:std::true_type{};\n'
   f'static_assert(HasMuse<CoreS3AudioCodec>::value == {int(bool(muse and voice))});\n')
  subprocess.run(['clang++','-std=c++17','-fsyntax-only','-Wall','-Wextra','-Werror',f'-DSTACKCHAN_BUILD_MUSE={muse}',f'-DSTACKCHAN_BUILD_MUSE_VOICE_PROTOCOL={voice}','-I'+str(r/'muse-codec-fixtures'),'-I'+str(r.parent/'main'),str(cpp)],check=True)
print('PASS actual CoreS3 native binding and Muse/voice-on/off API closure; no physical IO')
