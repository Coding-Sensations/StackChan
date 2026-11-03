"""Actual CoreS3 runtime method bodies with explicit SDK fatal-check doubles.
Proves numeric recording order/scope and existing return behavior, not hardware.
"""
import pathlib,subprocess,tempfile,unittest
repo=pathlib.Path(__file__).resolve().parents[3]
tests=pathlib.Path(__file__).resolve().parent
def body(source,signature):
    start=source.index(signature);opening=source.index('{',start);depth=0
    for i in range(opening,len(source)):
        if source[i]=='{':depth+=1
        elif source[i]=='}':
            depth-=1
            if depth==0:return source[start:i+1]
    raise AssertionError('unclosed method')
class CoreS3FaultTests(unittest.TestCase):
    def test_actual_driver_fault_precedes_unchanged_fatal_check_and_is_mira_scoped(self):
        source=(repo/'firmware/main/hal/board/cores3_audio_codec.cc').read_text()
        helpers=source[source.index('namespace {'):source.index('#include <esp_log.h>')]
        methods='\n'.join(body(source,s) for s in ['void CoreS3AudioCodec::EnableInput(bool enable)','void CoreS3AudioCodec::EnableOutput(bool enable)','int CoreS3AudioCodec::Read(int16_t* dest, int samples)','int CoreS3AudioCodec::Write(const int16_t* data, int samples)'])
        fixture=tests.joinpath('cores3-fault-fixture.hpp').read_text()
        with tempfile.TemporaryDirectory(prefix='stackchan-cores3-faults-') as directory:
            directory=pathlib.Path(directory);cpp=directory/'fixture.cpp';binary=directory/'fixture'
            cpp.write_text(fixture.replace('ACTUAL_HELPERS',helpers).replace('ACTUAL_METHODS',methods))
            subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(tests/'codec-fixture'),'-I'+str(repo/'firmware/main'),str(cpp),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True,timeout=10)
if __name__=='__main__':unittest.main()
