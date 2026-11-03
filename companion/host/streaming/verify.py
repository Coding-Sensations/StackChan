"""Finite compile/synthetic tests. Never starts CoreAudio, hardware or a network.
All outputs are exclusively owned tempfile artifacts. Preserve logs and hashes.
"""
import hashlib,json,os,pathlib,signal,subprocess,tempfile,time
root=pathlib.Path(__file__).resolve().parent
compiler='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'
sdk='/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX27.0.sdk'
started=time.monotonic(); deadline=started+60
report={'source_hashes':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in root.iterdir() if p.suffix in ('.cpp','.hpp','.py')},'commands':[]}
def rows():
    raw=subprocess.check_output(['ps','-axo','pid,ppid,pgid,rss,lstart'],text=True,timeout=min(2,max(.1,deadline-time.monotonic())))
    result=[]
    for line in raw.splitlines()[1:]:
        f=line.split()
        if len(f)==9: result.append((int(f[0]),int(f[2]),int(f[3]),' '.join(f[4:])))
    return result
with tempfile.TemporaryDirectory(prefix='stackchan-native-') as temp:
    out=pathlib.Path(temp)
    python='/Users/mikedemott/Homelab/.venv/bin/python'
    commands=[
        [python,'-B','-m','unittest','discover','-s',str(root),'-p','test_streaming.py','-v'],
        [python,'-B',str(root/'run_loopback.py')],
    ]
    commands=[([command[0],'-isysroot',sdk,'-isystem',sdk+'/usr/include/c++/v1',*command[1:]] if command[0]==compiler else command) for command in commands]
    try:
        for index,command in enumerate(commands):
            if time.monotonic()>deadline-6: raise TimeoutError('total work deadline')
            known={}; peak=0; errors=[]; t=time.monotonic()
            with (out/f'{index}.log').open('w') as log:
                p=subprocess.Popen(['nice','-n','20',*command],stdout=log,stderr=subprocess.STDOUT,start_new_session=True,env={**os.environ,'TMPDIR':temp,'PYTHONDONTWRITEBYTECODE':'1','PYTHONPATH':'/Users/mikedemott/Homelab/src:/Users/mikedemott/Developer/AgentWork/StackChan/bridge/host-adapter:/Users/mikedemott/Developer/AgentWork/StackChan/bridge/local-endpoint:/Users/mikedemott/Developer/AgentWork/StackChan/.tools/bridge-python'})
                try:
                    while True:
                        owned=[r for r in rows() if r[1]==p.pid]
                        for pid,group,rss,start in owned:
                            if pid in known and known[pid]!=start: raise RuntimeError('process identity changed')
                            known.setdefault(pid,start)
                        peak=max(peak,sum(r[2]*1024 for r in owned))
                        if p.poll() is not None: break
                        if time.monotonic()>min(t+20,deadline-6): raise TimeoutError('finite command deadline')
                        try:p.wait(timeout=.1)
                        except subprocess.TimeoutExpired:pass
                finally:
                    for sig in (signal.SIGTERM,signal.SIGKILL):
                        survivors=[r for r in rows() if r[1]==p.pid]
                        for pid,group,rss,start in survivors:
                            if known.get(pid)!=start: errors.append(f'unverified process {pid}'); continue
                            try:os.kill(pid,sig)
                            except ProcessLookupError:pass
                        if not survivors: break
                        try:p.wait(timeout=.2)
                        except subprocess.TimeoutExpired:pass
                    remaining=[r[0] for r in rows() if r[1]==p.pid]
            item={'argv':command,'exit':p.returncode,'seconds':time.monotonic()-t,'sampled_peak_tree_rss_bytes':peak,'retained_identities':known,'cleanup_errors':errors,'remaining_group_pids':remaining,'log':(out/f'{index}.log').read_text()}
            report['commands'].append(item)
            print(json.dumps(item),flush=True)
            if p.returncode or errors or remaining: raise RuntimeError('compile/test/closure failed')
        report['result']='PASS'
    except Exception as error:
        report['result']='FAIL'; report['error']=repr(error); raise
    finally:
        report['wall_seconds']=time.monotonic()-started
        (root/'VERIFICATION.json').write_text(json.dumps(report,indent=2)+'\n')
