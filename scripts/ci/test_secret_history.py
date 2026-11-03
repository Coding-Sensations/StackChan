"""Real Git/Gitleaks fixtures. All secret material is generated, synthetic and never printed."""
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

SCRIPT = Path(__file__).with_name('scan_secret_history.py')
TOOL = os.environ.get('STACKCHAN_GITLEAKS_BIN', '')


class HistoryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='secret-history-fixture-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.repo = self.root / 'repo'
        self.repo.mkdir()
        self.env = dict(os.environ, GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL=os.devnull,
                        GIT_AUTHOR_NAME='Fixture', GIT_AUTHOR_EMAIL='fixture@example.invalid',
                        GIT_COMMITTER_NAME='Fixture', GIT_COMMITTER_EMAIL='fixture@example.invalid')
        self.git('init', '-q', '--initial-branch=main')
        cfg = self.root / 'rules.toml'
        cfg.write_text('[[rules]]\nid="ci-synthetic"\ndescription="generated fixture"\n'
                       "regex='''SCANNER_FIXTURE_[A-F0-9]{32}'''\n")
        self.env['GITLEAKS_CONFIG'] = str(cfg)
        self.base = self.commit({'safe.txt': b'ordinary content\n'})
        self.ref('refs/heads/main', self.base)
        self.reports = self.root / 'reports'
        self.secrets = []

    def git(self, *args, data=None):
        p = subprocess.run(['git', '-C', str(self.repo), *args], input=data,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=self.env)
        if p.returncode:
            self.fail('fixture Git setup failed: ' + args[0])
        return p.stdout.decode().strip()

    def token(self, label):
        value = 'SCANNER_' + 'FIXTURE_' + hashlib.sha256(label.encode()).hexdigest()[:32].upper()
        self.secrets.append(value)
        return value.encode()

    def commit(self, files, parents=()):
        self.git('read-tree', '--empty')
        for name, content in files.items():
            blob = self.git('hash-object', '-w', '--stdin', data=content)
            self.git('update-index', '--add', '--cacheinfo', '100644,' + blob + ',' + name)
        tree = self.git('write-tree')
        args = ['commit-tree', tree]
        for parent in parents:
            args += ['-p', parent]
        return self.git(*args, data=b'synthetic fixture\n')

    def ref(self, name, commit):
        self.git('update-ref', name, commit)

    def octopus(self, files=None, count=3, parent_files=None):
        parents = []
        for i in range(count):
            contents = {'safe.txt': b'ordinary content\n', 'branch-' + str(i): b'safe\n'}
            if parent_files:
                contents.update(parent_files(i))
            parent = self.commit(contents, [self.base])
            self.ref('refs/heads/parent-' + str(i), parent)
            parents.append(parent)
        return self.commit(files or {'safe.txt': b'ordinary content\n'}, parents)

    def run_scan(self, expected, tool=None):
        p = subprocess.run([sys.executable, str(SCRIPT), '--repo', str(self.repo),
                            '--gitleaks', tool or TOOL, '--reports', str(self.reports)],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=self.env)
        for secret in self.secrets:
            self.assertNotIn(secret.encode(), p.stdout + p.stderr)
        self.assertEqual(p.returncode, expected, 'unexpected scan classification; no raw scanner output emitted')
        return p

    def findings(self, prefix):
        return [f for p in self.reports.glob(prefix + '*.json')
                for f in (json.loads(p.read_text()) or [])]

    def fake_tool(self, body):
        p = self.root / 'scanner-proxy'
        p.write_text('#!' + sys.executable + '\nimport sys,json,pathlib\n'
                     'if sys.argv[1:] == ["version"]:\n print("8.30.1");sys.exit(0)\n' + body)
        p.chmod(0o700)
        return str(p)

    def test_clean_octopus_and_ordinary_history_succeed(self):
        self.ref('refs/heads/main', self.octopus({'safe.txt': b'changed clean result\n'}))
        self.run_scan(0)
        self.assertTrue(list(self.reports.glob('octopus-*.json')))

    def test_parent_edges_and_blob_identity_are_recorded(self):
        merge = self.octopus({'result.txt': b'whole clean file\n'}, count=6)
        self.ref('refs/heads/main', merge)
        self.run_scan(0)
        coverage = json.loads((self.reports / 'coverage.json').read_text())
        self.assertEqual(len(coverage), 1)
        self.assertEqual(coverage[0]['original_commit'], merge)
        self.assertEqual(len(coverage[0]['parents']), 6)
        self.assertEqual(coverage[0]['blobs'], [{'path': 'result.txt', 'mode': '100644',
                                               'oid': self.git('rev-parse', merge + ':result.txt')}])

    def test_no_added_result_blobs_is_supported(self):
        self.ref('refs/heads/main', self.octopus())
        self.run_scan(0)
        self.assertEqual(json.loads((self.reports / 'coverage.json').read_text())[0]['blobs'], [])

    def test_resolution_only_secret_removed_later_is_detected(self):
        merge = self.octopus({'resolution.txt': self.token('resolution')})
        self.ref('refs/heads/main', self.commit({'safe.txt': b'clean later\n'}, [merge]))
        self.run_scan(1)
        self.assertTrue(any(f['Commit'] == merge and f['File'] == 'resolution.txt'
                            for f in self.findings('octopus-')))

    def test_all_parent_contents_are_compared_not_only_first_parent(self):
        files = {'parent-' + str(i) + '.txt': self.token('parent-' + str(i)) for i in range(4)}
        merge = self.octopus(files, count=4, parent_files=lambda i: {'parent-' + str(i) + '.txt': files['parent-' + str(i) + '.txt']})
        self.ref('refs/heads/main', merge)
        self.run_scan(1)
        found = {f['File'] for f in self.findings('octopus-') if f['Commit'] == merge}
        self.assertEqual(found, set(files))

    def test_remote_branch_octopus_is_included(self):
        merge = self.octopus({'side.txt': self.token('remote')})
        self.ref('refs/remotes/origin/side', merge)
        self.run_scan(1)
        self.assertTrue(any(f['Commit'] == merge for f in self.findings('octopus-')))

    def test_tag_only_octopus_is_included(self):
        merge = self.octopus({'tag.txt': self.token('tag')})
        self.ref('refs/tags/reviewed', merge)
        self.run_scan(1)
        self.assertTrue(any(f['Commit'] == merge for f in self.findings('octopus-')))

    def test_detached_head_octopus_is_included(self):
        merge = self.octopus({'detached.txt': self.token('detached')})
        (self.repo / '.git/HEAD').write_text(merge + '\n')
        self.run_scan(1)
        self.assertTrue(any(f['Commit'] == merge for f in self.findings('octopus-')))

    def test_binary_attribute_and_nul_do_not_hide_resolution_text(self):
        merge = self.octopus({'.gitattributes': b'*.dat -diff\n',
                              'payload.dat': b'\0prefix\n' + self.token('binary') + b'\n'})
        self.ref('refs/heads/main', merge)
        self.run_scan(1)
        self.assertTrue(any(f['File'] == 'payload.dat' for f in self.findings('octopus-')))

    def test_multiline_resolution_combines_content_from_different_parents(self):
        cfg = self.root / 'rules.toml'
        cfg.write_text('[[rules]]\nid="ci-multiline"\ndescription="generated multiline fixture"\n'
                       "regex='''(?s)BEGIN_MERGE_PAIR\\nVALUE_[A-F0-9]{32}\\nEND_MERGE_PAIR'''\n")
        body = ('VALUE_' + hashlib.sha256(b'multiline-fixture').hexdigest()[:32].upper()).encode()
        complete = b'BEGIN_MERGE_PAIR\n' + body + b'\nEND_MERGE_PAIR'
        self.secrets.append(complete.decode())
        self.secrets.append(body.decode())
        a = self.commit({'pair.txt': b'BEGIN_MERGE_PAIR\n'}, [self.base])
        b = self.commit({'pair.txt': body + b'\nEND_MERGE_PAIR'}, [self.base])
        c = self.commit({'pair.txt': b'BEGIN_MERGE_PAIR\n' + body + b'\n'}, [self.base])
        merge = self.commit({'pair.txt': complete}, [a, b, c])
        self.ref('refs/heads/main', merge)
        self.run_scan(1)
        self.assertTrue(any(f['Commit'] == merge for f in self.findings('octopus-')))

    def test_ordinary_two_parent_resolution_retains_remerge_coverage(self):
        a = self.commit({'a': b'safe\n'}, [self.base])
        b = self.commit({'b': b'safe\n'}, [self.base])
        merge = self.commit({'resolved.txt': self.token('two-parent')}, [a, b])
        self.ref('refs/heads/main', merge)
        self.run_scan(1)
        self.assertTrue(any(f['Commit'] == merge for f in self.findings('history')))
        self.assertFalse(list(self.reports.glob('octopus-*.json')))

    def test_unmerged_ordinary_branch_secret_is_detected(self):
        branch = self.commit({'branch.txt': self.token('ordinary')}, [self.base])
        self.ref('refs/remotes/origin/unmerged', branch)
        self.run_scan(1)
        self.assertTrue(any(f['Commit'] == branch for f in self.findings('history')))

    def test_shallow_history_fails_closed(self):
        (self.repo / '.git/shallow').write_text(self.base + '\n')
        self.run_scan(2)
        self.assertFalse(list(self.reports.glob('history.json')))

    def test_replace_refs_fail_closed(self):
        replacement = self.commit({'other': b'no credential\n'})
        self.ref('refs/replace/' + self.base, replacement)
        self.run_scan(2)

    def test_legacy_grafts_fail_closed(self):
        (self.repo / '.git/info/grafts').write_text(self.base + '\n')
        self.run_scan(2)

    def test_missing_blob_fails_before_scanner(self):
        blob = self.git('rev-parse', self.base + ':safe.txt')
        (self.repo / '.git/objects' / blob[:2] / blob[2:]).unlink()
        self.run_scan(2)
        self.assertFalse(list(self.reports.glob('history.json')))

    def test_missing_scanner_fails_closed(self):
        self.run_scan(2, str(self.root / 'missing'))

    def test_unsupported_object_format_fails_closed(self):
        self.repo = self.root / 'sha256-repo'
        self.repo.mkdir()
        self.git('init', '-q', '--object-format=sha256')
        self.run_scan(2)

    def test_changed_gitlink_fails_closed(self):
        original = self.octopus({'result': b'safe\n'})
        parents = self.git('rev-list', '--parents', '-n', '1', original).split()[1:]
        self.git('update-index', '--add', '--cacheinfo', '160000,' + self.base + ',external-module')
        args = ['commit-tree', self.git('write-tree')]
        for parent in parents:
            args += ['-p', parent]
        merge = self.git(*args, data=b'synthetic gitlink fixture\n')
        self.ref('refs/heads/main', merge)
        self.run_scan(2)

    def test_historical_policy_files_cannot_hide_resolution_content(self):
        merge = self.octopus({'.gitleaks.toml': b'title="not the selected policy"\n',
                              '.gitleaksignore': b'*\n', 'payload': self.token('policy')})
        self.ref('refs/heads/main', merge)
        self.run_scan(1)
        self.assertTrue(any(f['Commit'] == merge for f in self.findings('octopus-')))

    def test_report_status_mismatch_fails_closed(self):
        tool = self.fake_tool('pathlib.Path(sys.argv[sys.argv.index("--report-path")+1]).write_text("[]")\nsys.exit(1)\n')
        self.run_scan(2, tool)

    def test_non_list_report_fails_closed(self):
        tool = self.fake_tool('pathlib.Path(sys.argv[sys.argv.index("--report-path")+1]).write_text("{}")\nsys.exit(0)\n')
        self.run_scan(2, tool)

    def test_malformed_finding_fields_fail_closed(self):
        tool = self.fake_tool('pathlib.Path(sys.argv[sys.argv.index("--report-path")+1]).write_text(json.dumps([{"Commit":42}]))\nsys.exit(1)\n')
        self.run_scan(2, tool)

    def test_history_error_still_attempts_octopus_and_fails(self):
        self.ref('refs/heads/main', self.octopus())
        tool = self.fake_tool('p=pathlib.Path(sys.argv[sys.argv.index("--report-path")+1]);p.write_text("[]")\n'
                              'if p.name == "history.json": print("ERR incomplete",file=sys.stderr)\nsys.exit(0)\n')
        self.run_scan(2, tool)
        self.assertTrue(list(self.reports.glob('octopus-*.json')))

    def test_secondary_error_cannot_be_hidden_by_clean_history(self):
        self.ref('refs/heads/main', self.octopus())
        tool = self.fake_tool('p=pathlib.Path(sys.argv[sys.argv.index("--report-path")+1]);p.write_text("[]")\n'
                              'if p.name != "history.json": print("ERR incomplete",file=sys.stderr)\nsys.exit(0)\n')
        self.run_scan(2, tool)

    def test_ref_change_during_scan_fails_closed(self):
        tool = self.fake_tool('import subprocess\np=pathlib.Path(sys.argv[sys.argv.index("--report-path")+1]);p.write_text("[]")\n'
                              'subprocess.run(["git","-C",' + repr(str(self.repo)) + ',"update-ref","refs/heads/moved",' + repr(self.base) + '],check=True)\nsys.exit(0)\n')
        self.run_scan(2, tool)

    def test_shallow_state_appearing_during_scan_fails_closed(self):
        tool = self.fake_tool('p=pathlib.Path(sys.argv[sys.argv.index("--report-path")+1]);p.write_text("[]")\n'
                              'pathlib.Path(' + repr(str(self.repo / '.git/shallow')) + ').write_text(' + repr(self.base + '\n') + ')\nsys.exit(0)\n')
        self.run_scan(2, tool)

    def test_symlink_target_bytes_are_scanned_without_following(self):
        value = self.token('link')
        original = self.octopus({'link': value})
        parents = self.git('rev-list', '--parents', '-n', '1', original).split()[1:]
        self.git('update-index', '--cacheinfo', '120000,' + self.git('rev-parse', original + ':link') + ',link')
        args = ['commit-tree', self.git('write-tree')]
        for parent in parents:
            args += ['-p', parent]
        merge = self.git(*args, data=b'synthetic link fixture\n')
        self.ref('refs/heads/main', merge)
        self.run_scan(1)
        self.assertEqual(json.loads((self.reports / 'coverage.json').read_text())[0]['blobs'][0]['mode'], '120000')
        self.assertTrue(any(f['File'] == 'link' and f['Commit'] == merge for f in self.findings('octopus-')))

    def test_only_exact_existing_original_fingerprint_is_allowed(self):
        merge = self.octopus({'payload.txt': self.token('allowed')})
        self.ref('refs/heads/main', merge)
        (self.repo / '.gitleaksignore').write_text(merge + ':payload.txt:ci-synthetic:1\n')
        self.run_scan(0)
        self.assertEqual(len(json.loads((self.reports / 'octopus-000.json').read_text())), 1)
        self.assertEqual(json.loads((self.reports / 'octopus-000.mapped.json').read_text()), [])

    def test_nonmatching_fingerprint_does_not_allow_resolution(self):
        merge = self.octopus({'payload.txt': self.token('not-allowed')})
        self.ref('refs/heads/main', merge)
        (self.repo / '.gitleaksignore').write_text(self.base + ':payload.txt:ci-synthetic:1\n')
        self.run_scan(1)

    def test_scanner_timeout_fails_closed(self):
        spec = importlib.util.spec_from_file_location('secret_scan_test', SCRIPT)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        tool = self.fake_tool('import time\ntime.sleep(1)\n')
        with mock.patch.object(module, 'SCAN_TIMEOUT', 0.05):
            with self.assertRaises(module.Incomplete):
                module.scan(self.repo, Path(tool), self.reports)

    def test_empty_repository_fails_closed(self):
        self.repo = self.root / 'empty-repo'
        self.repo.mkdir()
        self.git('init', '-q')
        self.run_scan(2)

    def test_scanner_errors_fail_closed_even_with_zero_exit(self):
        tool = self.fake_tool('p=pathlib.Path(sys.argv[sys.argv.index("--report-path")+1]);p.write_text("[]")\nprint("ERR incomplete parser",file=sys.stderr)\nsys.exit(0)\n')
        self.run_scan(2, tool)

    def test_missing_report_fails_closed(self):
        self.run_scan(2, self.fake_tool('sys.exit(0)\n'))

    def test_malformed_report_fails_closed(self):
        tool = self.fake_tool('pathlib.Path(sys.argv[sys.argv.index("--report-path")+1]).write_text("{")\nsys.exit(0)\n')
        self.run_scan(2, tool)

    def test_unsupported_exit_fails_closed(self):
        self.run_scan(2, self.fake_tool('sys.exit(7)\n'))

    def test_different_tool_version_fails_closed(self):
        tool = self.root / 'wrong-version'
        tool.write_text('#!' + sys.executable + '\nprint("8.0.0")\n')
        tool.chmod(0o700)
        self.run_scan(2, str(tool))


if __name__ == '__main__':
    if not TOOL:
        raise SystemExit('STACKCHAN_GITLEAKS_BIN must identify the verified pinned test binary; no tests skipped')
    unittest.main()
