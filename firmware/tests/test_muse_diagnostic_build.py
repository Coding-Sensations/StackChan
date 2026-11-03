from pathlib import Path
import subprocess,tempfile,os,json
r=Path(__file__).resolve().parent;fw=r.parent
helper=fw/'main/build_profiles/muse_diagnostic.cmake'
# Real CMake policy, not a replacement resolver.
cmake=os.environ.get('CMAKE_COMMAND','cmake')
with tempfile.TemporaryDirectory() as tmp:
 t=Path(tmp)
 for fault in range(12):
  script=t/'check.cmake'
  base={'STACKCHAN_MUSE_LOCAL_DIAGNOSTIC':'ON','STACKCHAN_PROFILE_MUSE':'1','STACKCHAN_PROFILE_MUSE_VOICE_PROTOCOL':'1','STACKCHAN_MUSE_RAM_ONLY':'ON','CONFIG_BOARD_TYPE_M5STACK_STACK_CHAN':'ON'}
  bad=[('STACKCHAN_MUSE_LOCAL_DIAGNOSTIC','OFF'),('STACKCHAN_PROFILE_MUSE','0'),('STACKCHAN_PROFILE_MUSE_VOICE_PROTOCOL','0'),('STACKCHAN_MUSE_RAM_ONLY','OFF'),('CONFIG_BOARD_TYPE_M5STACK_STACK_CHAN','OFF'),('STACKCHAN_COMPANION_REMEMBERED_RUNTIME','ON'),('STACKCHAN_COMPANION_WIRELESS_PAIRING','ON'),('STACKCHAN_COMPANION_TRUSTED_USB_PROVISIONING','ON'),('STACKCHAN_COMPANION_BINDING_CONFIG','not-a-secret'),('STACKCHAN_ENABLE_LOCAL_PORTRAITS','ON'),('STACKCHAN_COMPANION_LOCAL_CA_FILE','not-a-secret'),('STACKCHAN_COMPANION_LOCAL_TLS_HOST','192.0.2.1')]
  if fault:base.update([bad[fault]])
  text='cmake_minimum_required(VERSION 3.16)\nset(SOURCES companion/muse_local_diagnostic.cc companion/muse_cold_output.cc)\n'+'\n'.join(f'set({k} {v})' for k,v in base.items())+f'\ninclude("{helper.as_posix()}")\n'
  script.write_text(text);p=subprocess.run([cmake,'-P',script],capture_output=True,text=True)
  assert (p.returncode==0)==(fault==0),(fault,p.stdout,p.stderr)
 # Default-OFF source is absent, not merely hidden in the UI.
 script.write_text(f'cmake_minimum_required(VERSION 3.16)\nset(SOURCES companion/muse_local_diagnostic.cc keep.cc)\ninclude("{helper.as_posix()}")\nif(STACKCHAN_DIAGNOSTIC_ENABLED OR "companion/muse_local_diagnostic.cc" IN_LIST SOURCES)\nmessage(FATAL_ERROR "default diagnostic inclusion")\nendif()\n')
 p=subprocess.run([cmake,'-P',script],capture_output=True,text=True);assert p.returncode==0,p.stdout+p.stderr
print('PASS actual default-off diagnostic source/configuration policy')

presets={p['name']:p for p in json.loads((fw/'CMakePresets.json').read_text())['configurePresets']}
assert presets['muse-ram-reference']['cacheVariables'].get('STACKCHAN_MUSE_LOCAL_DIAGNOSTIC')=='OFF'
assert presets['muse-cold-diagnostic']['cacheVariables']['STACKCHAN_MUSE_LOCAL_DIAGNOSTIC']=='ON'
assert presets['muse-cold-diagnostic']['binaryDir']!=presets['muse-ram-reference']['binaryDir']
print('PASS named normal preset clears sticky diagnostic flag; diagnostic output directory is separate')

assert presets['muse-cold-diagnostic']['cacheVariables']['SDKCONFIG'].endswith('/build-muse-cold-diagnostic/sdkconfig')
root_cmake=(fw/'CMakeLists.txt').read_text()
preflight=root_cmake[root_cmake.index('# Diagnostic overlay preflight'):root_cmake.index('# Auto-load a local sdkconfig overlay')]
with tempfile.TemporaryDirectory() as tmp:
 t=Path(tmp);(t/'sdkconfig.defaults.local').write_text('synthetic overlay only')
 for enabled in ['ON','OFF']:
  script=t/'guard.cmake';script.write_text('set(CMAKE_CURRENT_SOURCE_DIR "'+str(t)+'")\nset(STACKCHAN_MUSE_LOCAL_DIAGNOSTIC '+enabled+')\n'+preflight)
  p=subprocess.run([cmake,'-P',script],capture_output=True,text=True)
  assert (p.returncode!=0)==(enabled=='ON'),p.stdout+p.stderr
print('PASS actual root preflight rejects implicit local SDK overlays before loading them')

# Compose actual root preflight and actual main trust-generator call. A wrapper
# marks any invocation before delegating to the real generator with synthetic data.
import sys
main_cmake=(fw/'main/CMakeLists.txt').read_text();start=main_cmake.index('  execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../../companion/configure_local_tls.py"');end=main_cmake.index('  if(NOT COMPANION_TRUST_RESULT EQUAL 0)',start)
generator=main_cmake[start:end].replace('${CMAKE_CURRENT_LIST_DIR}',str(fw/'main'))
with tempfile.TemporaryDirectory() as tmp:
 t=Path(tmp);binding=t/'synthetic-binding.json';binding.write_text('{}');marker=t/'generator-ran'
 wrapper=t/'python-wrapper';wrapper.write_text('#!'+sys.executable+'\nfrom pathlib import Path\nimport subprocess,sys\nPath('+repr(str(marker))+').write_text("called")\nraise SystemExit(subprocess.call([sys.executable,*sys.argv[1:]]))\n');wrapper.chmod(0o700)
 for enabled in ['OFF','ON']:
  marker.unlink(missing_ok=True);script=t/'order.cmake'
  script.write_text('cmake_minimum_required(VERSION 3.16)\nset(CMAKE_CURRENT_SOURCE_DIR "'+str(t)+'")\nset(CMAKE_CURRENT_BINARY_DIR "'+str(t)+'")\nset(STACKCHAN_MUSE_LOCAL_DIAGNOSTIC '+enabled+')\nset(STACKCHAN_COMPANION_BINDING_CONFIG "'+str(binding)+'")\nset(PYTHON "'+str(wrapper)+'")\nset(COMPANION_TRUST_SOURCE "'+str(t/'trust.cc')+'")\n'+preflight+'\n'+generator)
  p=subprocess.run([cmake,'-P',script],capture_output=True,text=True)
  assert marker.exists()==(enabled=='OFF'),(enabled,p.stdout,p.stderr)
  if enabled=='ON':assert p.returncode!=0
print('PASS forbidden diagnostic binding rejected before actual trust-generator invocation')
