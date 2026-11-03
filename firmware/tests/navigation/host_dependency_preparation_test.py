"""Exercise production fixture preparation with exact local inputs and no network."""
from contextlib import ExitStack
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('prepare', ROOT / 'scripts/ci/prepare_host_dependencies.py')
prepare = importlib.util.module_from_spec(spec)
if ref := os.environ.get('STACKCHAN_PREPARE_TEST_REF'):
    original = subprocess.check_output(['git', 'show', ref + ':scripts/ci/prepare_host_dependencies.py'], cwd=ROOT)
    exec(compile(original, str(ROOT / 'scripts/ci/prepare_host_dependencies.py'), 'exec'), prepare.__dict__)
else:
    spec.loader.exec_module(prepare)
REAL_RUN = subprocess.run


class PreparationTests(unittest.TestCase):
    def setUp(self):
        # macOS /var is itself a symlink; production deliberately rejects
        # symlinked destination ancestors. Use this task's real workspace.
        self.temporary = tempfile.TemporaryDirectory(prefix='stackchan-prepare-test-', dir=ROOT.parent)
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        raw = self.directory / 'raw'
        self.downloads = {}
        for repository, revision, destination, files in prepare.SOURCES:
            for relative in files:
                source = ROOT / 'firmware' / destination / relative
                target = raw / destination / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
        # Reconstruct raw pinned inputs from the checked fixture copies. The
        # real selected patch must be applied again by production preparation.
        REAL_RUN(['git', 'apply', '-R', '--include=main/application.cc', '--include=main/ota.cc',
                  str(ROOT / 'firmware/patches/xiaozhi-e77dedb.patch')],
                 cwd=raw / 'xiaozhi-esp32', check=True)
        for repository, revision, destination, files in prepare.SOURCES:
            for relative in files:
                url = f'https://raw.githubusercontent.com/{repository}/{revision}/{relative}'
                self.downloads[url] = (raw / destination / relative).read_bytes()
        self.target = self.directory / 'target'
        (self.target / 'patches').mkdir(parents=True)
        shutil.copyfile(ROOT / 'firmware/patches/xiaozhi-e77dedb.patch',
                        self.target / 'patches/xiaozhi-e77dedb.patch')
        self.fetches = []

    def run_preparation(self):
        def download(url, timeout):
            self.assertEqual(timeout, 30)
            return io.BytesIO(self.downloads[url])

        def run(argv, **kwargs):
            if argv[:2] == ['git', 'apply']:
                return REAL_RUN(argv, **kwargs)
            self.fetches.append(argv)
            return subprocess.CompletedProcess(argv, 0)

        with ExitStack() as stack:
            stack.enter_context(patch.object(prepare, 'FIRMWARE', self.target))
            stack.enter_context(patch.object(prepare, 'urlopen', download))
            stack.enter_context(patch.object(prepare.subprocess, 'run', run))
            stack.enter_context(patch.object(prepare.subprocess, 'check_output',
                                             return_value='572a7e488641f1c3f22e85d65c27f095ecf27d12\n'))
            prepare.main()

    def test_all_reviewed_patched_sources_are_prepared(self):
        self.run_preparation()
        for _, _, destination, files in prepare.SOURCES:
            for relative, expected in files.items():
                self.assertEqual(hashlib.sha256((self.target / destination / relative).read_bytes()).hexdigest(), expected)
        self.assertEqual(len(self.fetches), 3)

    def test_corrupt_input_rejects_before_any_install(self):
        url = next(url for url in self.downloads if url.endswith('/dns_server.cc'))
        self.downloads[url] += b'\n// changed source\n'
        with self.assertRaisesRegex(SystemExit, 'Unreviewed host dependency'):
            self.run_preparation()
        self.assertFalse((self.target / 'xiaozhi-esp32').exists())
        self.assertFalse(self.fetches)

    def test_differing_existing_source_is_preserved(self):
        existing = self.target / 'xiaozhi-esp32/main/application.cc'
        existing.parent.mkdir(parents=True)
        existing.write_text('different local file')
        with self.assertRaisesRegex(SystemExit, 'Refusing to replace'):
            self.run_preparation()
        self.assertEqual(existing.read_text(), 'different local file')
        self.assertEqual(list(existing.parent.iterdir()), [existing])
        self.assertFalse(self.fetches)


if __name__ == '__main__':
    unittest.main()
