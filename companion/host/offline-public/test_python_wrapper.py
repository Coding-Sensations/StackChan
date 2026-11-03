"""Exercise shell path selection with a recording interpreter, never test IO."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SCRIPT=Path(__file__).resolve().parents[1]/'numeric-ui/script/test_python_offline.sh'


class PythonWrapper(unittest.TestCase):
    def run_wrapper(self, *, private=True, valid=True, skip=False):
        temporary=tempfile.TemporaryDirectory(prefix='wrapper path with spaces ')
        self.addCleanup(temporary.cleanup)
        base=Path(temporary.name);root=base/'public checkout';host=root/'companion/host'
        script=host/'numeric-ui/script/test_python_offline.sh';script.parent.mkdir(parents=True)
        shutil.copyfile(SCRIPT,script)
        streaming=host/'streaming';streaming.mkdir()
        for name in ('test_synthetic.py','test_remote_gateway.py'):(streaming/name).write_text('raise AssertionError("must never execute")\n')
        home=base/'explicit private source'
        (home/'src/home_lab' if valid else home).mkdir(parents=True)
        receipt=base/'receipt.json';interpreter=base/'recording python'
        interpreter.write_text('''#!/bin/sh
exec python3 - "$@" <<'PY'
import json,os,sys
from pathlib import Path
Path(os.environ['WRAPPER_RECEIPT']).write_text(json.dumps(dict(args=sys.argv[1:],path=os.environ.get('PYTHONPATH'),cwd=os.getcwd())))
PY
''');interpreter.chmod(0o700)
        env=dict(os.environ,STACKCHAN_PYTHON=str(interpreter),WRAPPER_RECEIPT=str(receipt))
        env.pop('HOME_LAB_ROOT',None);env.pop('STACKCHAN_SKIP_MODULES',None)
        if private:env['HOME_LAB_ROOT']=str(home)
        if skip:env['STACKCHAN_SKIP_MODULES']='test_synthetic'
        result=subprocess.run(['bash',str(script)],cwd=base,env=env,capture_output=True,text=True,timeout=10)
        return result,receipt,root,home

    def test_uses_current_checkout_and_explicit_private_source_with_spaces(self):
        result,receipt,root,home=self.run_wrapper()
        self.assertEqual(result.returncode,0,result.stderr)
        value=json.loads(receipt.read_text());paths=value['path'].split(os.pathsep)
        self.assertEqual(paths,[str(home/'src'),str(root/'bridge/host-adapter'),str(root/'bridge/local-endpoint'),str(root/'companion/host/native-audio'),str(root/'companion/host/streaming'),str(root/'companion/host')])
        self.assertEqual(value['cwd'],str((root/'companion/host/streaming').resolve()))
        self.assertEqual(value['args'],['-B','-m','unittest','test_synthetic'])
        self.assertIn('SKIPPED test_remote_gateway',result.stderr)

    def test_missing_private_root_fails_before_interpreter(self):
        result,receipt,_,_=self.run_wrapper(private=False)
        self.assertNotEqual(result.returncode,0);self.assertFalse(receipt.exists())

    def test_invalid_private_root_fails_before_interpreter(self):
        result,receipt,_,_=self.run_wrapper(valid=False)
        self.assertNotEqual(result.returncode,0);self.assertFalse(receipt.exists())

    def test_empty_selected_suite_cannot_fall_back_to_listener_discovery(self):
        result,receipt,_,_=self.run_wrapper(skip=True)
        self.assertNotEqual(result.returncode,0);self.assertFalse(receipt.exists())


if __name__=='__main__':unittest.main()
