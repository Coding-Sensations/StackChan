from pathlib import Path
import importlib.util
root=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('profile',root/'tools/build_profile.py')
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
key='muse_voice_protocol'
assert key in p.FEATURES, 'voice protocol must have its own pre-flash closure'
for name in ('full','no-games'):
 r=p.resolve(name);assert r['definitions']['STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL']==1
assert p.resolve('mira-core')['definitions']['STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL']==0
for selected in ([],['muse'],['muse',key]):
 r=p.resolve('custom',{'schema':1,'features':selected});assert r['definitions']['STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL']==int(key in selected)
try:p.resolve('custom',{'schema':1,'features':[key]})
except ValueError as e:assert 'requires muse' in str(e).lower()
else:raise AssertionError('voice protocol without Muse must fail closed')
r=p.resolve('custom',{'schema':1,'features':['muse',key]})
assert 'set(STACKCHAN_PROFILE_MUSE_VOICE_PROTOCOL 1)' in p.render_cmake(r)
assert '#define STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL 0' in (root/'main/build_profiles/features.hpp').read_text()
print('Voice profile dependency and seven closure cases passed')

# Execute the actual component CMake closure with inert IDF target commands.
# Muse-off and voice-off must exclude both source units and the link root.
import os, shutil, subprocess, tempfile
cmake=os.environ.get('CMAKE_COMMAND') or shutil.which('cmake')
assert cmake, 'CMake required to exercise actual optional component closure'
component=root/'components/muse_direct_protocol/CMakeLists.txt'
for muse,voice in [(0,0),(1,0),(1,1)]:
 with tempfile.TemporaryDirectory(prefix='voice-profile-cmake-') as temporary:
  script=Path(temporary)/'check.cmake'
  script.write_text(f'''set(STACKCHAN_PROFILE_MUSE {muse})
set(STACKCHAN_PROFILE_MUSE_VOICE_PROTOCOL {voice})
function(idf_component_register)
 message("REGISTER=${{ARGV}}")
endfunction()
function(target_compile_features)
endfunction()
function(target_link_libraries)
 message("LINK=${{ARGV}}")
endfunction()
include("{component.as_posix()}")
''')
  result=subprocess.run([cmake,'-P',str(script)],capture_output=True,text=True)
  assert result.returncode==0,result.stdout+result.stderr
  output=result.stdout+result.stderr
  for source in ['voice/service_exchange.cc','voice/exchange_api.cc','stackchan_muse_voice_exchange_api']:
   assert (source in output)==bool(muse and voice),(muse,voice,source,output)
print('Actual component CMake includes exchange sources/link root only with Muse + voice selected')
