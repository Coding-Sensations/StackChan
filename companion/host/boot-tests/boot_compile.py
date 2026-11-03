"""One-worker, network-denied compile; no flashing or external SDK downloads."""
import hashlib,json,os,pathlib,signal,subprocess,time
root=pathlib.Path('/Users/mikedemott/Developer/AgentWork/StackChan');firmware=root/'src/upstream/firmware'
lock=firmware/'dependencies.lock';before=hashlib.sha256(lock.read_bytes()).hexdigest()
sources={str(p.relative_to(firmware)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (firmware/'main/companion').iterdir() if p.is_file()}
log=root/'.tools/companion-boot-compile.log';start=time.monotonic();deadline=start+180
command=['/usr/bin/sandbox-exec','-p','(version 1)(allow default)(deny network*)','/bin/zsh','-c','source /Users/mikedemott/Developer/AgentWork/StackChan/.tools/sdk/esp-idf-v5.5.4/export.sh && cmake --build build --target lang_header --parallel 1 && cmake --build build --parallel 1']
env={**os.environ,'IDF_TOOLS_PATH':str(root/'.tools/espressif'),'PIP_CACHE_DIR':str(root/'.tools/pip-cache'),'CMAKE_BUILD_PARALLEL_LEVEL':'1'}
def rows():
    raw=subprocess.check_output(['ps','-axo','pid,ppid,pgid,rss,lstart'],text=True,timeout=min(2,max(.1,deadline-time.monotonic())))
    result=[]
    for line in raw.splitlines()[1:]:
        f=line.split()
        if len(f)==9:result.append((int(f[0]),int(f[2]),int(f[3]),' '.join(f[4:])))
    return result
known={};peak=0;reason=None;errors=[]
with log.open('w') as stream:
    p=subprocess.Popen(['nice','-n','20',*command],cwd=firmware,env=env,stdout=stream,stderr=subprocess.STDOUT,start_new_session=True)
    try:
        while True:
            owned=[r for r in rows() if r[1]==p.pid]
            for pid,group,rss,stamp in owned:
                if pid in known and known[pid]!=stamp:raise RuntimeError('PID identity changed')
                known.setdefault(pid,stamp)
            peak=max(peak,sum(r[2]*1024 for r in owned))
            if p.poll() is not None:break
            if time.monotonic()>deadline-8:raise TimeoutError('finite compile deadline')
            try:p.wait(timeout=.2)
            except subprocess.TimeoutExpired:pass
    except Exception as error:reason=repr(error)
    finally:
        for sig in (signal.SIGTERM,signal.SIGKILL):
            survivors=[r for r in rows() if r[1]==p.pid]
            for pid,group,rss,stamp in survivors:
                if known.get(pid)!=stamp:errors.append(f'unverified process {pid}');continue
                try:os.kill(pid,sig)
                except ProcessLookupError:pass
            if not survivors:break
            try:p.wait(timeout=.5)
            except subprocess.TimeoutExpired:pass
        remaining=[r[0] for r in rows() if r[1]==p.pid]
after=hashlib.sha256(lock.read_bytes()).hexdigest();success=p.returncode==0 and not reason and not errors and not remaining and before==after
result={'result':'PASS' if success else 'FAIL','exit':p.returncode,'reason':reason,'argv':command,'wall_seconds':time.monotonic()-start,'sampled_peak_tree_rss_bytes':peak,'retained_identities':known,'cleanup_errors':errors,'remaining_group_pids':remaining,'source_hashes':sources,'dependency_lock_sha256_before':before,'dependency_lock_sha256_after':after,'network_denied':True,'workers':1,'log':str(log),'runtime_activation':'custom boot/UI entry compiled; physical activation not executed','flashed':False}
if success:
    binary=firmware/'build/stack-chan.bin';result['binary_bytes']=binary.stat().st_size;result['binary_sha256']=hashlib.sha256(binary.read_bytes()).hexdigest()
(pathlib.Path(__file__).parent/'BOOT-FIRMWARE-VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result));print(log.read_text()[-6000:])
raise SystemExit(0 if success else 1)
