"""Actual firmware TickView/socket Tick/Page Back sequence; fake SDK boundaries only.
No USB, socket, codec, LVGL driver, listener, grant, or firmware build is used.
"""
import pathlib, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[3]
def body(source, signature):
    start=source.index(signature);opening=source.index('{',start);depth=0;quoted=None;escaped=False
    for pos in range(opening,len(source)):
        c=source[pos]
        if quoted:
            if escaped:escaped=False
            elif c=='\\':escaped=True
            elif c==quoted:quoted=None
        elif c in ('"',"'"):quoted=c
        elif c=='{':depth+=1
        elif c=='}':
            depth-=1
            if not depth:return source[start:pos+1]
    raise AssertionError('unclosed source body')
class TickCleanupTests(unittest.TestCase):
    def test_codec_stop_blocks_heartbeat_until_cleanup_without_navigation(self):
        boot=(ROOT/'firmware/main/companion/boot.cc').read_text()
        socket=(ROOT/'firmware/main/companion/companion_socket.cc').read_text()
        page=(ROOT/'firmware/main/apps/stock_shell/stock_apps.cc').read_text()
        fixture=pathlib.Path(__file__).with_name('tick_cleanup_fixture.hpp').read_text()
        actual_tick=body(boot,'void TickView()')
        actual_socket=body(socket,'void CompanionSocket::Tick()')
        actual_back=body(page,'bool Back(int action)')
        app=page[page.index('class CompanionApp'):]
        actual_running=body(app,'void onRunning() override').replace(' override','')
        stopped='[](uint32_t,bool){mute_requested=true;}'
        self.assertIn(stopped,boot)
        code=fixture.replace('ACTUAL_SOCKET_TICK',actual_socket).replace('ACTUAL_TICK_VIEW',actual_tick).replace('ACTUAL_PAGE_BACK',actual_back).replace('ACTUAL_APP_RUNNING',actual_running).replace('ACTUAL_STOPPED_CALLBACK',stopped)
        with tempfile.TemporaryDirectory(prefix='stackchan-tick-stall-offline-') as folder:
            source=pathlib.Path(folder)/'fixture.cpp';binary=pathlib.Path(folder)/'fixture'
            source.write_text(code)
            build=subprocess.run(['clang++','-std=c++17','-pthread','-Wall','-Wextra','-I',str(ROOT/'firmware/main'),str(source),'-o',str(binary)],capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stderr)
            subprocess.run([str(binary)],check=True,timeout=5,capture_output=True)
if __name__=='__main__':unittest.main()
