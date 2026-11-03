"""Bounded supervisor for audited pilot commands; observes/kills only owned processes.
Sampling is not an atomic OS memory quota. Child jobs must not daemonize, setsid,
change pgrp, or detach; descendant group changes are an error. No RLIMIT_AS claim.
"""
import ctypes
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time

class TaskInfo(ctypes.Structure):
    _fields_=[(n,ctypes.c_uint64) for n in ['virtual','resident','total_user','total_system','threads_user','threads_system']]+[(n,ctypes.c_int32) for n in ['policy','faults','pageins','cow','msgsent','msgreceived','machcalls','unixcalls','csw','threadnum','numrunning','priority']]

class BsdInfo(ctypes.Structure):
    _fields_=[('head',ctypes.c_uint32*12),('comm',ctypes.c_char*16),('name',ctypes.c_char*32),('tail',ctypes.c_uint32*6),('start_sec',ctypes.c_uint64),('start_usec',ctypes.c_uint64)]

class StepBudget:
    def __init__(self,path,limit):
        if not 1<=limit<=100: raise ValueError('invalid step cap')
        self.path=Path(path);self.limit=limit;self.granted=0
        self.path.write_text('0')
    def before_step(self):
        if self.granted>=self.limit: raise RuntimeError('step cap reached before next step')
        self.granted+=1
        temporary=self.path.with_suffix('.tmp');temporary.write_text(str(self.granted));temporary.replace(self.path)
        return self.granted

class MacOwnedMonitor:
    def __init__(self):
        self.tracked={}
        self.lib=ctypes.CDLL('/usr/lib/libproc.dylib',use_errno=True)
        self.lib.proc_listpids.argtypes=[ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_int]
        self.lib.proc_pidinfo.argtypes=[ctypes.c_int,ctypes.c_int,ctypes.c_uint64,ctypes.c_void_p,ctypes.c_int]
    def ids(self,kind,target):
        count=self.lib.proc_listpids(kind,target,None,0)
        if count<0: raise RuntimeError('owned pid enumeration failed')
        buffer=(ctypes.c_int*max(64,count//4+32))()
        read=self.lib.proc_listpids(kind,target,buffer,ctypes.sizeof(buffer))
        if read<0: raise RuntimeError('owned pid read failed')
        return {x for x in buffer[:read//4] if x>0}
    def identity(self,pid):
        info=BsdInfo()
        read=self.lib.proc_pidinfo(pid,3,0,ctypes.byref(info),ctypes.sizeof(info))
        if read!=ctypes.sizeof(info): return None
        return (info.start_sec,info.start_usec)
    def owned_alive(self,pgid):
        candidates=self.ids(2,pgid)
        for pid in candidates:
            identity=self.identity(pid)
            if identity: self.tracked.setdefault(pid,identity)
        return {pid for pid,identity in self.tracked.items() if self.identity(pid)==identity}
    def sample(self,pgid):
        pids=self.owned_alive(pgid)
        if not pids: return {'rss':0,'threads':0,'running':0,'pids':[]}
        rss=threads=running=0
        for pid in pids:
            try:
                if os.getpgid(pid)!=pgid: raise RuntimeError('pgrp changed')
            except ProcessLookupError: continue
            info=TaskInfo()
            size=self.lib.proc_pidinfo(pid,4,0,ctypes.byref(info),ctypes.sizeof(info))
            if size!=ctypes.sizeof(info):
                try: os.kill(pid,0)
                except ProcessLookupError: continue
                raise RuntimeError('owned RSS/thread telemetry unavailable')
            # Look only for children of already owned processes.
            for child in self.ids(6,pid):
                identity=self.identity(child)
                if identity: self.tracked.setdefault(child,identity)
                try:
                    if os.getpgid(child)!=pgid:
                        raise RuntimeError('owned child escaped process group; command prohibited')
                except ProcessLookupError: pass
            rss+=info.resident; threads+=info.threadnum; running+=info.numrunning
        return {'rss':rss,'threads':threads,'running':running,'pids':sorted(pids)}

def signal_owned_group(monitor,pgid,sig):
    # macOS host policy rejects negative-PID killpg; enumerate this owned group
    # and verify membership immediately before individually signalling members.
    # No global process inventory and no unrelated PID are used.
    for pid in monitor.owned_alive(pgid):
        try:
            if monitor.identity(pid)==monitor.tracked.get(pid): os.kill(pid,sig)
        except ProcessLookupError: pass

def workspace_bytes(root):
    total=0
    for base, dirs, files in os.walk(root,followlinks=False):
        dirs[:]=[d for d in dirs if not Path(base,d).is_symlink()]
        for filename in files:
            path=Path(base,filename)
            try:
                st=path.lstat()
                if not path.is_symlink(): total+=max(st.st_size,st.st_blocks*512)
            except FileNotFoundError: pass
    return total

def breach(metrics, limits):
    for key in ['rss','threads','workspace_bytes','elapsed']:
        if metrics[key]>limits[key]: return key
    if metrics['free_bytes']<limits['free_bytes']: return 'drive_floor'
    if metrics.get('steps',0)>limits['steps']: return 'steps'
    return None

def bounded_env(root):
    env=os.environ.copy()
    for name in ['OMP_NUM_THREADS','OPENBLAS_NUM_THREADS','MKL_NUM_THREADS','NUMEXPR_NUM_THREADS','VECLIB_MAXIMUM_THREADS','TF_NUM_INTRAOP_THREADS','TF_NUM_INTEROP_THREADS','UV_CONCURRENT_DOWNLOADS','UV_CONCURRENT_BUILDS','UV_CONCURRENT_INSTALLS']:
        env[name]='1'
    env.update({'TMPDIR':str(root/'tmp'),'UV_CACHE_DIR':str(root/'cache'/'uv'),'PIP_CACHE_DIR':str(root/'cache'/'pip'),'HF_HOME':str(root/'cache'/'huggingface'),'TORCH_HOME':str(root/'cache'/'torch'),'XDG_CACHE_HOME':str(root/'cache'),'CUDA_VISIBLE_DEVICES':'-1','PYTHONDONTWRITEBYTECODE':'1'})
    return env

def run(command,root,receipt,limits,step_file=None,interval=.25,monitor=None):
    root=Path(root).resolve(); receipt=Path(receipt)
    for d in ['tmp','cache','logs']: (root/d).mkdir(exist_ok=True)
    initial={'rss':0,'threads':0,'workspace_bytes':workspace_bytes(root),'elapsed':0,'free_bytes':shutil.disk_usage(root).free,'steps':0}
    reason=breach(initial,limits)
    result={'command':command,'limits':limits,'started':False,'reason':reason,'samples':0,'peak_rss':0,'peak_threads':0,'exit_code':None,'group_reaped':False}
    if reason:
        receipt.write_text(json.dumps(result,indent=2)+'\n'); return result
    monitor=monitor or MacOwnedMonitor()
    start=time.monotonic()
    def priority(): os.nice(10)
    with (root/'logs'/f'{receipt.stem}.stdout.txt').open('w') as out, (root/'logs'/f'{receipt.stem}.stderr.txt').open('w') as err:
        process=subprocess.Popen(command,cwd=root,env=bounded_env(root),start_new_session=True,preexec_fn=priority,stdout=out,stderr=err)
        pgid=process.pid
        result.update({'started':True,'owned_pgid':pgid})
        try:
            while process.poll() is None:
                sample=monitor.sample(pgid)
                if (process.pid not in sample['pids'] or sample['rss']<=0) and process.poll() is None:
                    raise RuntimeError('direct owned PID/RSS missing while child remains live')
                sample.update({'workspace_bytes':workspace_bytes(root),'elapsed':time.monotonic()-start,'free_bytes':shutil.disk_usage(root).free,'steps':0})
                if step_file and Path(step_file).exists():
                    sample['steps']=int(Path(step_file).read_text().strip())
                result['samples']+=1
                result['peak_rss']=max(result['peak_rss'],sample['rss'])
                result['peak_threads']=max(result['peak_threads'],sample['threads'])
                result['last_sample']=sample
                reason=breach(sample,limits)
                if reason: result['reason']=reason; break
                time.sleep(interval)
        except (Exception,KeyboardInterrupt) as error:
            result['reason']='telemetry_or_interrupt'; result['error']=str(error)
        finally:
            result['work_elapsed']=time.monotonic()-start
            # Always clean up this group, including descendants left after parent exits.
            cleanup_errors=[]
            try:
                signal_owned_group(monitor,pgid,signal.SIGTERM)
            except Exception as error: cleanup_errors.append(str(error))
            finally:
                # Authoritative direct-child cleanup must survive telemetry failure.
                if process.poll() is None: process.terminate()
            deadline=time.monotonic()+2
            while time.monotonic()<deadline:
                process.poll()
                try:
                    if not monitor.owned_alive(pgid): break
                except Exception as error:
                    cleanup_errors.append(str(error));break
                time.sleep(.05)
            try:
                signal_owned_group(monitor,pgid,signal.SIGKILL)
            except Exception as error: cleanup_errors.append(str(error))
            finally:
                if process.poll() is None: process.kill()
                result['exit_code']=process.wait(timeout=5)
            deadline=time.monotonic()+2
            try:
                while monitor.owned_alive(pgid) and time.monotonic()<deadline: time.sleep(.05)
                result['group_reaped']=not bool(monitor.owned_alive(pgid)) and not cleanup_errors
            except Exception as error:
                cleanup_errors.append(str(error));result['group_reaped']=False
            result['owned_birth_identities']={str(pid):list(identity) for pid,identity in monitor.tracked.items()}
            if cleanup_errors:
                result['cleanup_errors']=cleanup_errors
                result['reason']=result['reason'] or 'descendant_custody_uncertain'
    result['elapsed']=time.monotonic()-start
    result['workspace_bytes']=workspace_bytes(root)
    result['free_bytes']=shutil.disk_usage(root).free
    final_metrics={'rss':result['peak_rss'],'threads':result['peak_threads'],'workspace_bytes':result['workspace_bytes'],'elapsed':result['work_elapsed'],'free_bytes':result['free_bytes'],'steps':int(Path(step_file).read_text()) if step_file and Path(step_file).exists() else 0}
    result['final_metrics']=final_metrics
    result['reason']=result['reason'] or breach(final_metrics,limits)
    receipt.write_text(json.dumps(result,indent=2)+'\n')
    return result

DEFAULT={'rss':16000000000,'threads':32,'workspace_bytes':1024**3,'free_bytes':200*1024**3,'elapsed':7200,'steps':100}
if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--receipt',required=True);p.add_argument('--step-file');p.add_argument('--seconds',type=float,default=7200);p.add_argument('--rss',type=int,default=16000000000);p.add_argument('--threads',type=int,default=32);p.add_argument('command',nargs=argparse.REMAINDER);a=p.parse_args()
    limits=dict(DEFAULT,elapsed=a.seconds,rss=a.rss,threads=a.threads)
    command=a.command[1:] if a.command[:1]==['--'] else a.command
    root=Path(__file__).resolve().parents[1]
    r=run(command,root,root/a.receipt,limits,step_file=root/a.step_file if a.step_file else None)
    print(json.dumps(r,indent=2));raise SystemExit(0 if r['exit_code']==0 and not r['reason'] and r['group_reaped'] else 1)
