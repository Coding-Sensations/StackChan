"""Exercise the real build helper with inert compiler/sign/launch commands."""
import json
import os
from pathlib import Path
import plistlib
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

UI = Path(__file__).resolve().parents[1] / 'numeric-ui'


class AppBundleRoot(unittest.TestCase):
    def build(self, mode, *, override=False, missing=False, cdpath=False, relative_script=False, decoy_cdpath=False):
        temporary = tempfile.TemporaryDirectory(prefix="bundle & <root> ' ")
        self.addCleanup(temporary.cleanup)
        base = Path(temporary.name).resolve()
        root = base / 'checkout'
        ui = root / 'companion/host/numeric-ui'
        (ui / 'script').mkdir(parents=True)
        (ui / 'Resources').mkdir()
        for name in ('build_and_run.sh', 'package_resources.sh', 'verify_bundle.sh'):
            shutil.copyfile(UI / 'script' / name, ui / 'script' / name)
        shutil.copyfile(UI / 'Resources/mira-idle-frame.png', ui / 'Resources/mira-idle-frame.png')
        recorder = base / 'commands.py'
        recorder.write_text('''import json, os, plistlib, sys
from pathlib import Path
kind, *args = sys.argv[1:]
bundle = Path(os.environ['STACKCHAN_UI_BUILD_ROOT']) / 'StackChanPairing.app'
metadata = bundle / 'Contents/Info.plist'
if kind == 'nice':
 output = Path(args[args.index('-o') + 1]); output.write_text('inert fixture; never executed'); output.chmod(0o700)
elif kind == 'plutil':
 data = plistlib.loads(metadata.read_bytes())
 if args[0] == '-insert':
  assert args[:3] == ['-insert', 'StackChanRoot', '-string']
  data[args[1]] = args[3]; metadata.write_bytes(plistlib.dumps(data))
 elif args[0] != '-lint': raise AssertionError(args)
elif kind not in ('open', 'codesign'): raise AssertionError(kind)
value = dict(kind=kind, args=args)
if metadata.exists(): value['root'] = plistlib.loads(metadata.read_bytes()).get('StackChanRoot')
with open(os.environ['BUNDLE_RECEIPT'], 'a') as stream: stream.write(json.dumps(value) + '\\n')
''')
        shims = base / 'inert commands.sh'
        command = shlex.quote(sys.executable) + ' ' + shlex.quote(str(recorder))
        shims.write_text('\n'.join(
            f'function /usr/bin/{name} {{ {command} {name} "$@"; }}'
            for name in ('nice', 'plutil', 'codesign', 'open')) + '\n')
        receipt = base / 'receipt.jsonl'
        build = base / 'output'
        env = dict(os.environ, BASH_ENV=str(shims), STACKCHAN_UI_BUILD_ROOT=str(build),
                   BUNDLE_RECEIPT=str(receipt), PYTHONDONTWRITEBYTECODE='1')
        env.pop('STACKCHAN_ROOT', None)
        selected = root
        if override or missing:
            selected = base / 'explicit & <checkout> " root'
            if not missing: selected.mkdir()
            env['STACKCHAN_ROOT'] = str(selected)
        caller = base / 'caller'
        caller.mkdir()
        if cdpath:
            selected = base / 'relative checkout'
            selected.mkdir()
            env['STACKCHAN_ROOT'] = selected.name
            env['CDPATH'] = str(base)
        script = str(ui / 'script/build_and_run.sh')
        if relative_script:
            caller = root
            env['CDPATH'] = str(root)
            script = 'companion/host/numeric-ui/script/build_and_run.sh'
            if decoy_cdpath:
                decoy = base / 'different checkout'
                shutil.copytree(ui, decoy / 'companion/host/numeric-ui')
                env['CDPATH'] = str(decoy)
        result = subprocess.run(['/bin/bash', script, mode],
                                env=env, cwd=caller, capture_output=True, text=True, timeout=15)
        records = [json.loads(line) for line in receipt.read_text().splitlines()] if receipt.exists() else []
        metadata = build / 'StackChanPairing.app/Contents/Info.plist'
        return result, records, metadata, selected

    def test_derived_root_persists_in_unsigned_bundle_without_launch(self):
        result, records, metadata, root = self.build('--compile-only')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(plistlib.loads(metadata.read_bytes()).get('StackChanRoot'), str(root))
        self.assertFalse(any(r['kind'] in ('open', 'codesign') for r in records))

    def test_explicit_escaped_root_is_available_before_sign_and_launch(self):
        result, records, metadata, root = self.build('run', override=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(plistlib.loads(metadata.read_bytes()).get('StackChanRoot'), str(root))
        consumers = [r for r in records if r['kind'] in ('codesign', 'open')]
        self.assertEqual([r['kind'] for r in consumers], ['codesign', 'codesign', 'open'])
        self.assertTrue(all(r.get('root') == str(root) for r in consumers))

    def test_relative_checkout_from_cdpath_persists_exact_canonical_root(self):
        result, records, metadata, root = self.build('--compile-only', cdpath=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(plistlib.loads(metadata.read_bytes()).get('StackChanRoot'), str(root))
        self.assertFalse(any(r['kind'] in ('open', 'codesign') for r in records))

    def test_relative_helper_invocation_with_cdpath_preserves_derived_root(self):
        result, records, metadata, root = self.build('--compile-only', relative_script=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(plistlib.loads(metadata.read_bytes()).get('StackChanRoot'), str(root))
        self.assertFalse(any(r['kind'] in ('open', 'codesign') for r in records))

    def test_cdpath_cannot_select_sources_from_a_different_checkout(self):
        result, records, metadata, root = self.build('--compile-only', relative_script=True, decoy_cdpath=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(plistlib.loads(metadata.read_bytes()).get('StackChanRoot'), str(root))
        self.assertFalse(any(r['kind'] in ('open', 'codesign') for r in records))

    def test_missing_checkout_fails_before_compilation_or_launch(self):
        result, records, _, _ = self.build('--compile-only', missing=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(records, [])


if __name__ == '__main__': unittest.main()
