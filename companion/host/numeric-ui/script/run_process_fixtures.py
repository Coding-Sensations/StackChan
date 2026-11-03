"""Bounded macOS supervisor for the three audited, no-fork Foundation fixtures.

This is a separate policy from fixture_watchdog's strict single-process-group rule.
It admits birth/lineage-verified children in the root session, across PGIDs. Every
Python test body cooperatively stops before admission. It is a sampled supervisor,
not an OS sandbox or protection against hostile/double-forking programs.
"""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import secrets
import signal
import stat
import sys
import time

import fixture_watchdog as guard

FIXTURES = ('LifecycleChecks', 'CompanionChecks', 'PersistentChecks')
WATCHDOG_SHA = '687061c3ff3e2b5088637fbb908e1d53de365ab7b5408c53600c5cf76ac0cf74'


class CustodyError(RuntimeError):
    pass


class Kernel:
    def __init__(self):
        self.base = guard.MacOwnedMonitor()

    def info(self, pid):
        value = guard.BsdInfo()
        n = self.base.lib.proc_pidinfo(pid, 3, 0, ctypes.byref(value), ctypes.sizeof(value))
        if n != ctypes.sizeof(value):
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                return None
            raise CustodyError('live process identity unavailable')
        if value.head[3] != pid:
            raise CustodyError('kernel PID identity mismatch')
        try:
            sid = os.getsid(pid)
        except ProcessLookupError:
            return None
        return dict(birth=(value.start_sec, value.start_usec), parent=value.head[4],
                    sid=sid, pgid=value.tail[1], status=value.head[1])

    def children(self, pid):
        return self.base.ids(6, pid)

    def task(self, pid):
        value = guard.TaskInfo()
        n = self.base.lib.proc_pidinfo(pid, 4, 0, ctypes.byref(value), ctypes.sizeof(value))
        if n != ctypes.sizeof(value):
            if self.info(pid) is None:
                return None
            raise CustodyError('live process RSS/thread telemetry unavailable')
        return value.resident, value.threadnum, value.numrunning

    def send(self, pid, sig):
        os.kill(pid, sig)


def read_regular(path, max_size=2048):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        s = os.fstat(fd)
        if not stat.S_ISREG(s.st_mode) or s.st_uid != os.getuid() or s.st_size > max_size:
            raise CustodyError('invalid registration file')
        data = os.read(fd, max_size + 1)
        if len(data) > max_size:
            raise CustodyError('oversized registration')
        return data
    finally:
        os.close(fd)


class FoundationMonitor:
    def __init__(self, directory, nonce, active, kernel=None):
        self.kernel = kernel or Kernel()
        self.directory, self.nonce, self.active = Path(directory), nonce, Path(active)
        self.tracked = {}
        self.parents = {}
        self.root = None
        self.admitted = {}
        self.first_seen = {}
        self.violations = []

    def identity(self, pid):
        info = self.kernel.info(pid)
        return info['birth'] if info else None

    def violation(self, message):
        self.violations.append(message)
        raise CustodyError(message)

    def owned_alive(self, root):
        if self.root is None:
            info = self.kernel.info(root)
            if not info or info['sid'] != root or info['pgid'] != root:
                self.violation('root identity/session unavailable')
            self.root = root
            self.tracked[root] = info['birth']
        elif self.root != root:
            self.violation('root changed')
        alive = set()
        queue = list(self.tracked)
        visited = set()
        while queue:
            pid = queue.pop()
            if pid in visited:
                continue
            visited.add(pid)
            info = self.kernel.info(pid)
            if not info or info['birth'] != self.tracked[pid]:
                continue  # Never signal or adopt a reused PID.
            alive.add(pid)
            # Retain custody on failure; cleanup still needs these exact births.
            if info['sid'] != root:
                message = 'owned process escaped session'
                if message not in self.violations:
                    self.violations.append(message)
            for child in self.kernel.children(pid):
                ci = self.kernel.info(child)
                parent_now = self.kernel.info(pid)
                if not ci:
                    continue
                if not parent_now or parent_now['birth'] != self.tracked[pid] or ci['parent'] != pid:
                    self.violations.append('child lineage uncertain')
                    continue
                if child in self.tracked and self.tracked[child] != ci['birth']:
                    self.violations.append('owned child PID reused')
                    continue
                self.tracked[child] = ci['birth']
                self.parents.setdefault(child, (pid, self.tracked[pid]))
                self.first_seen.setdefault(child, time.monotonic())
                queue.append(child)
        self.persist(alive)
        return alive

    def persist(self, alive):
        snapshot = dict(root=self.root, owned_birth_identities={str(p): list(b) for p, b in self.tracked.items()},
                        lineage={str(p): [q, list(b)] for p, (q, b) in self.parents.items()},
                        observed_alive=sorted(alive), admitted=self.admitted, violations=self.violations)
        temporary = self.active.with_suffix('.tmp')
        temporary.write_text(json.dumps(snapshot, indent=2) + '\n')
        temporary.replace(self.active)

    def admit(self, alive):
        for request in self.directory.glob('*.request'):
            ticket = request.stem
            if len(ticket) != 36 or any(c not in '0123456789ABCDEF-' for c in ticket):
                self.violation('invalid ticket')
            try:
                data = json.loads(read_regular(request))
            except (OSError, ValueError) as error:
                self.violation('invalid request: ' + str(error))
            if set(data) != {'ticket', 'nonce', 'pid', 'ppid', 'sid'} or data['ticket'] != ticket or data['nonce'] != self.nonce:
                self.violation('request identity/nonce mismatch')
            if read_regular(self.directory / (ticket + '.planned')) != ticket.encode():
                self.violation('missing launch intent')
            pid = data['pid']
            if type(pid) is not int or pid not in self.tracked or pid == self.root:
                self.violation('request has no verified child lineage')
            if ticket in self.admitted:
                old = self.admitted[ticket]
                if old['pid'] != pid or old['request'] != data:
                    self.violation('replayed registration')
                continue
            info = self.kernel.info(pid)
            parent = self.parents[pid]
            pi = self.kernel.info(parent[0])
            if pid not in alive or not info or info['birth'] != self.tracked[pid]:
                self.violation('registration writer vanished or reused')
            if info['parent'] != parent[0] or data['ppid'] != parent[0] or not pi or pi['birth'] != parent[1]:
                self.violation('registration parent unavailable or changed')
            if info['sid'] != self.root or data['sid'] != self.root:
                self.violation('registration session mismatch')
            if info['status'] != 4:  # Darwin SSTOP; request write precedes self-stop.
                if time.monotonic() - self.first_seen[pid] > 2:
                    self.violation('registration stop timeout')
                continue
            entry = dict(pid=pid, birth=list(info['birth']), parent=parent[0], sid=info['sid'],
                         pgid=info['pgid'], request=data)
            self.admitted[ticket] = entry
            # Persist verified custody before authorizing CONT; no JSON-only adoption.
            self.persist(alive)
            marker = self.directory / (ticket + '.admitted')
            fd = os.open(marker, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
            os.close(fd)
            if self.identity(pid) != self.tracked[pid]:
                self.violation('birth changed before CONT')
            self.kernel.send(pid, signal.SIGCONT)

    def sample(self, root):
        alive = self.owned_alive(root)
        if self.violations:
            raise CustodyError(self.violations[-1])
        self.admit(alive)
        admitted_pids = {v['pid'] for v in self.admitted.values()}
        for pid in alive - {root}:
            if self.parents[pid][0] != root:
                self.violation('unaudited grandchild')
            if pid not in admitted_pids and time.monotonic() - self.first_seen[pid] > 2:
                self.violation('unregistered child timeout')
        rss = threads = running = 0
        for pid in alive:
            if self.identity(pid) != self.tracked[pid]:
                continue
            task = self.kernel.task(pid)
            if task:
                rss += task[0]; threads += task[1]; running += task[2]
        return dict(rss=rss, threads=threads, running=running, pids=sorted(alive))

    def verify_complete(self):
        planned = {p.stem for p in self.directory.glob('*.planned')}
        requests = {p.stem for p in self.directory.glob('*.request')}
        if list(self.directory.glob('*.pending')):
            self.violation('unfinished registration write')
        admitted_births = [(v['pid'], tuple(v['birth'])) for v in self.admitted.values()]
        observed_births = {(pid, birth) for pid, birth in self.tracked.items() if pid != self.root}
        if len(admitted_births) != len(set(admitted_births)) or set(admitted_births) != observed_births:
            self.violation('observed child lacks unique admission')
        if not planned or planned != requests or planned != set(self.admitted):
            self.violation('incomplete fixture admission/registration')
        if self.violations:
            raise CustodyError(self.violations[-1])


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary-dir', type=Path, required=True)
    parser.add_argument('--workspace', type=Path, action='append', required=True)
    parser.add_argument('--pins', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('fixture', choices=FIXTURES)
    a = parser.parse_args()
    if hashlib.sha256(Path(guard.__file__).read_bytes()).hexdigest() != WATCHDOG_SHA:
        raise CustodyError('base watchdog source changed')
    binary = (a.binary_dir / a.fixture).resolve(strict=True)
    pins = json.loads(a.pins.read_text())
    if pins.get(a.fixture) != hashlib.sha256(binary.read_bytes()).hexdigest():
        raise CustodyError('fixture binary pin mismatch')
    roots = [p.resolve(strict=True) for p in a.workspace]
    work = a.binary_dir.resolve(strict=True)
    if not any(work == r or r in work.parents for r in roots) or any(x == y or x in y.parents or y in x.parents for i,x in enumerate(roots) for y in roots[i+1:]):
        raise CustodyError('workspace roots must include output and be disjoint')
    receipt_path = a.receipt.resolve()
    if not any(r in receipt_path.parents for r in roots):
        raise CustodyError('receipt/active paths must be covered by workspace budget')
    a.receipt = receipt_path
    a.receipt.parent.mkdir(parents=True, exist_ok=True)
    directory = work / ('custody-' + secrets.token_hex(12)); directory.mkdir(mode=0o700)
    nonce = secrets.token_hex(32)
    monitor = FoundationMonitor(directory, nonce, a.receipt.with_suffix('.active.json'))
    original_env = guard.bounded_env
    guard.bounded_env = lambda root: dict(original_env(root), SC_FIXTURE_CUSTODY_DIR=str(directory), SC_FIXTURE_CUSTODY_NONCE=nonce)
    measure = guard.workspace_bytes
    guard.workspace_bytes = lambda unused: sum(measure(r) for r in roots)
    # No priority/security setting change; supervisor remains sampled like the original.
    guard.os.nice = lambda delta: None
    limits = dict(guard.DEFAULT, rss=2*1024**3, threads=8, elapsed=300, workspace_bytes=1024**3)
    result = guard.run([str(binary)], work, a.receipt, limits, interval=.025, monitor=monitor)
    try:
        monitor.verify_complete()
    except Exception as error:
        result['reason'] = result['reason'] or 'custody_incomplete'
        result['custody_error'] = str(error)
    result.update(policy='audited-no-fork-Foundation-fixtures-v1', custody_directory=str(directory),
                  admitted_workers=monitor.admitted, custody_violations=monitor.violations,
                  combined_workspaces=[str(p) for p in roots])
    a.receipt.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k:result.get(k) for k in ['exit_code','reason','group_reaped','custody_error','peak_rss','peak_threads','elapsed','workspace_bytes']}))
    return 0 if result['exit_code'] == 0 and not result['reason'] and result['group_reaped'] else 1


if __name__ == '__main__':
    sys.exit(main())
