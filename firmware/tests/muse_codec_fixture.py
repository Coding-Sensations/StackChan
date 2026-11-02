from pathlib import Path
import re
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
def production_fixture(test):
 prefix=(r/'muse_codec_fixture.hpp').read_text().replace('#pragma once\n','',1).replace('ACTUAL_METHODS',methods)
 source=(r/test).read_text()
 assert source.startswith('#include "muse_codec_fixture.hpp"\n')
 return prefix+source.split('\n',1)[1]
