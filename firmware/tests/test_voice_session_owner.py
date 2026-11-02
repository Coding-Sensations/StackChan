from pathlib import Path
import subprocess, tempfile, hashlib, json
r=Path(__file__).resolve().parent
c=r.parent/'components/muse_direct_protocol'
provenance=json.loads((r/'voice_exchange_crypto_fixture.PROVENANCE.json').read_text())
assert hashlib.sha256((r/'voice_exchange_crypto_fixture.hpp').read_bytes()).hexdigest()==provenance['fixture_sha256']
flags=['-I'+str(r),'-I'+str(r/'muse-codec-fixtures'),'-I'+str(r/'muse-pcm-fixtures'),'-DNOISE_BUILDING_TESTS','-DSTACKCHAN_BUILD_MUSE=1','-DSTACKCHAN_BUILD_MUSE_VOICE_PROTOCOL=1','-I'+str(r.parent/'main'),'-I'+str(r/'vendor/cjson'),'-g','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(c),'-I'+str(c/'pairing'),'-I'+str(c/'noise_core/include')]
with tempfile.TemporaryDirectory(prefix='voice-exchange-') as temp:
 t=Path(temp)
 from muse_codec_fixture import production_fixture
 cpp=t/'session.cpp';cpp.write_text(production_fixture('voice_session_owner_test.cpp'))
 commands=[['clang','-std=c11',*flags,'-c',str(c/'voice/muse_voice_call_core.c'),'-o',str(t/'core.o')],
 ['clang++','-std=c++17',*flags,str(cpp),str(c/'voice/service_exchange.cc'),str(c/'voice/exchange_api.cc'),*[str(c/'noise_core/src'/s) for s in ['ClientSession.cpp','InitiatorHandshake.cpp','ServiceCodec.cpp','Status.cpp','Transport.cpp','TransportFrameCodec.cpp']],str(t/'core.o'),'-o',str(t/'test')],[str(t/'test')]]
 for cmd in commands:
  p=subprocess.run(cmd,capture_output=True,text=True)
  assert p.returncode==0,p.stdout+p.stderr
print('PASS actual admitted SDK/Core/direction/session/UI retained cleanup; synthetic credentials/authority/ports only')
