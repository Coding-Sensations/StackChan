#!/usr/bin/env python3
"""Fail-closed full history plus explicit per-parent octopus resolution scanning."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HISTORY_OPTIONS = '--all --full-history --remerge-diff'
# Scratch roots contain whole changed result blobs, including unchanged line context.
OCTOPUS_OPTIONS = '--all --full-history --text --no-renames --no-ext-diff --no-textconv'
VERSION = '8.30.1'
SCAN_TIMEOUT = 120
OID = re.compile(r'^[0-9a-f]{40}$')
ERROR = re.compile(r'(^|\s)(ERR|FTL)(\s|$)')


class Incomplete(Exception):
    """Only constant stage labels, never scanner output or credential values."""


def git(repo, env, *args, data=None):
    try:
        p = subprocess.run(['git', '-C', str(repo), *args], env=env,
                           input=data, capture_output=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        raise Incomplete('Git command unavailable or timed out') from None
    if p.returncode:
        raise Incomplete('Git history inspection failed')
    try:
        return p.stdout.decode('utf-8')
    except UnicodeError:
        raise Incomplete('Unsupported Git metadata encoding') from None


def refs(repo, env):
    return (git(repo, env, 'rev-parse', '--verify', 'HEAD').strip(),
            git(repo, env, 'for-each-ref', '--format=%(refname) %(objectname)'))


def inspect_history(repo, env):
    if git(repo, env, 'rev-parse', '--is-shallow-repository').strip() != 'false':
        raise Incomplete('Shallow history is incomplete')
    if git(repo, env, 'rev-parse', '--show-object-format').strip() != 'sha1':
        raise Incomplete('Unsupported object format')
    if git(repo, env, 'for-each-ref', 'refs/replace/'):
        raise Incomplete('Replacement refs conceal original history')
    graft = Path(git(repo, env, 'rev-parse', '--git-path', 'info/grafts').strip())
    if not graft.is_absolute():
        graft = repo / graft
    if graft.exists() and any(x.strip() and not x.lstrip().startswith('#')
                              for x in graft.read_text().splitlines()):
        raise Incomplete('Legacy grafts conceal original history')
    snapshot = refs(repo, env)
    objects = git(repo, env, 'rev-list', '--objects', '--all', '--missing=print')
    if any(x.startswith('?') for x in objects.splitlines()):
        raise Incomplete('Reachable Git objects are missing')
    graph = {}
    for row in git(repo, env, 'rev-list', '--all', '--full-history', '--parents').splitlines():
        parts = row.split()
        if not parts or not all(OID.fullmatch(x) for x in parts):
            raise Incomplete('Unsupported parent graph')
        graph[parts[0]] = parts[1:]
    if not graph or snapshot[0] not in graph or any(p not in graph for ps in graph.values() for p in ps):
        raise Incomplete('Parent graph is incomplete')
    # Probe actual installed Git support, rather than assuming a version implies it.
    git(repo, env, 'log', '--all', '--full-history', '--remerge-diff', '--max-count=1', '--format=')
    octopus = {c: graph[c] for c in sorted(graph) if len(graph[c]) >= 3}
    if octopus:
        git(repo, env, 'log', '--no-walk', '--diff-merges=separate', '--text', '--no-renames',
            '--no-ext-diff', '--no-textconv', '--format=', next(iter(octopus)))
    return snapshot, octopus


def inspection_roots(repo, env, scratch, octopus, reports):
    """Read original objects; create isolated parentless inspection commits only."""
    git(scratch, env, 'init', '-q')
    objects = Path(git(repo, env, 'rev-parse', '--git-path', 'objects').strip())
    if not objects.is_absolute():
        objects = repo / objects
    if '\n' in str(objects) or '\r' in str(objects):
        raise Incomplete('Unsupported object directory encoding')
    (scratch / '.git/objects/info/alternates').write_text(str(objects.resolve()) + '\n')
    private_env = dict(env, GIT_AUTHOR_NAME='Content inspection',
                       GIT_AUTHOR_EMAIL='inspection@example.invalid',
                       GIT_COMMITTER_NAME='Content inspection',
                       GIT_COMMITTER_EMAIL='inspection@example.invalid',
                       GIT_AUTHOR_DATE='2000-01-01T00:00:00Z',
                       GIT_COMMITTER_DATE='2000-01-01T00:00:00Z')
    mapping, evidence = {}, []
    for number, (commit, parents) in enumerate(octopus.items()):
        changed, edges = set(), []
        for parent in parents:
            paths = git(repo, env, 'diff-tree', '-r', '--no-renames', '--name-only',
                        '-z', parent, commit, '--').split('\0')
            paths = {p for p in paths if p}
            changed.update(paths)
            edges.append({'parent': parent, 'changed_paths': len(paths)})
        entries = []
        for row in git(repo, env, 'ls-tree', '-r', '-z', commit).split('\0'):
            if not row:
                continue
            header, path = row.split('\t', 1)
            mode, kind, oid = header.split()
            if path not in changed:
                continue
            if kind != 'blob' or mode not in ('100644', '100755', '120000') or not OID.fullmatch(oid):
                raise Incomplete('Unsupported changed tree entry')
            entries.append((path, mode, oid))
        git(scratch, private_env, 'read-tree', '--empty')
        records = ''.join(mode + ' ' + oid + '\t' + path + '\0'
                          for path, mode, oid in entries).encode()
        git(scratch, private_env, 'update-index', '-z', '--index-info', data=records)
        tree = git(scratch, private_env, 'write-tree').strip()
        inspection = git(scratch, private_env, 'commit-tree', tree,
                         data=('Whole-file inspection of ' + commit + '\n').encode()).strip()
        git(scratch, private_env, 'update-ref', 'refs/heads/inspection-' + str(number), inspection)
        mapping[inspection] = commit
        evidence.append({'original_commit': commit, 'parents': edges, 'inspection_commit': inspection,
                         'inspection_tree': tree,
                         'blobs': [{'path': p, 'mode': m, 'oid': o} for p, m, o in entries]})
    (reports / 'coverage.json').write_text(json.dumps(evidence, indent=2) + '\n')
    return mapping


def scan_pass(repo, tool, env, reports, label, options, policy_repo=None, mapping=None):
    report = reports / (label + '.json')
    log = reports / (label + '.log')
    command = [str(tool), 'git', '--redact', '--no-banner', '--no-color', '--report-format=json',
               '--report-path', str(report), '--log-opts=' + options, str(repo)]
    policy_repo = policy_repo or repo
    command += ['--gitleaks-ignore-path', str(policy_repo)]
    if not env.get('GITLEAKS_CONFIG') and not env.get('GITLEAKS_CONFIG_TOML') and (policy_repo / '.gitleaks.toml').is_file():
        command += ['--config', str(policy_repo / '.gitleaks.toml')]
    try:
        with log.open('wb') as output:
            p = subprocess.run(command, cwd=repo, env=env, stdout=output,
                               stderr=subprocess.STDOUT, timeout=SCAN_TIMEOUT)
        text = log.read_text(errors='replace')
        if p.returncode not in (0, 1) or ERROR.search(text):
            raise Incomplete('Scanner error: ' + label)
        if not report.is_file() or report.stat().st_size > 20 * 1024 * 1024:
            raise Incomplete('Scanner report unavailable: ' + label)
        findings = json.loads(report.read_text())
        if findings is None:  # Gitleaks may serialize the empty finding slice as null.
            findings = []
        if not isinstance(findings, list) or any(not isinstance(x, dict) for x in findings):
            raise Incomplete('Unsupported scanner report: ' + label)
        if bool(findings) != (p.returncode == 1):
            raise Incomplete('Scanner status/report mismatch: ' + label)
        allowed = set()
        if mapping and (policy_repo / '.gitleaksignore').is_file():
            allowed = {x.strip() for x in (policy_repo / '.gitleaksignore').read_text().splitlines()
                       if x.strip() and not x.lstrip().startswith('#')}
        mapped = []
        for item in findings:
            if (not all(isinstance(item.get(k), str) and item[k] for k in
                        ('Commit', 'RuleID', 'File', 'Fingerprint')) or
                    type(item.get('StartLine')) is not int or item['StartLine'] < 1 or
                    not OID.fullmatch(item['Commit'])):
                raise Incomplete('Scanner finding lacks commit evidence: ' + label)
            if mapping:
                inspection = item['Commit']
                if inspection not in mapping or not item.get('Fingerprint', '').startswith(inspection + ':'):
                    raise Incomplete('Unbound inspection finding')
                item = dict(item, InspectionCommit=inspection, Commit=mapping[inspection],
                            Fingerprint=mapping[inspection] + item['Fingerprint'][len(inspection):])
                if item['Fingerprint'] in allowed:
                    continue
            mapped.append(item)
            # Never print Match, Secret, full raw logs or exception text.
            print(json.dumps({'pass': label, **{k: item.get(k) for k in
                  ('RuleID', 'File', 'StartLine', 'Commit', 'Fingerprint')}}, sort_keys=True))
        if mapping:
            (reports / (label + '.mapped.json')).write_text(json.dumps(mapped, indent=2) + '\n')
        print(label + ': completed; findings=' + str(len(mapped)))
        return int(bool(mapped))
    except (OSError, UnicodeError, ValueError, subprocess.TimeoutExpired):
        raise Incomplete('Scanner execution/report incomplete: ' + label) from None


def scan(repo, tool, reports):
    env = dict(os.environ, GIT_NO_REPLACE_OBJECTS='1', GIT_NO_LAZY_FETCH='1')
    for key in ('GIT_REPLACE_REF_BASE', 'GIT_DIR', 'GIT_COMMON_DIR', 'GIT_WORK_TREE', 'GIT_INDEX_FILE',
                'GIT_OBJECT_DIRECTORY', 'GIT_ALTERNATE_OBJECT_DIRECTORIES', 'GIT_SHALLOW_FILE'):
        env.pop(key, None)
    snapshot, octopus = inspect_history(repo, env)
    try:
        v = subprocess.run([str(tool), 'version'], capture_output=True, env=env, timeout=10)
    except (OSError, subprocess.TimeoutExpired):
        raise Incomplete('Pinned scanner unavailable') from None
    if v.returncode or v.stdout.decode(errors='replace').strip() != VERSION:
        raise Incomplete('Unsupported scanner version')
    reports.mkdir(parents=True, exist_ok=True)
    if any(reports.iterdir()):
        raise Incomplete('Reports destination is not fresh')
    result = 0
    incomplete = []
    try:
        result = scan_pass(repo, tool, env, reports, 'history', HISTORY_OPTIONS)
    except Incomplete as error:
        incomplete.append(str(error))
    if octopus:
        try:
            with tempfile.TemporaryDirectory(prefix='merge-content-inspection-') as temporary:
                scratch = Path(temporary)
                mapping = inspection_roots(repo, env, scratch, octopus, reports)
                result = max(result, scan_pass(scratch, tool, env, reports, 'octopus-000',
                                              OCTOPUS_OPTIONS, policy_repo=repo, mapping=mapping))
        except Incomplete as error:
            incomplete.append(str(error))
    final_snapshot, final_octopus = inspect_history(repo, env)
    if final_snapshot != snapshot or final_octopus != octopus:
        raise Incomplete('History changed during scan')
    if incomplete:
        raise Incomplete('; '.join(incomplete))
    print('Coverage complete: full history plus ' + str(len(octopus)) + ' octopus merge(s), all parents')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--gitleaks', type=Path, required=True)
    parser.add_argument('--reports', type=Path, required=True)
    args = parser.parse_args()
    try:
        return scan(args.repo.resolve(), args.gitleaks.resolve(), args.reports.resolve())
    except Incomplete as error:
        print('Secret history coverage incomplete: ' + str(error), file=sys.stderr)
        return 2
    except (OSError, UnicodeError, ValueError):
        print('Secret history coverage incomplete: inspection failure', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
