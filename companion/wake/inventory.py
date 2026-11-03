"""Read-only interpreter distribution metadata; never imports ML or audio packages."""
import importlib.metadata as md
import json
import platform
import shutil
import subprocess
import sys
from pathlib import Path
NAMES = ['openwakeword','onnxruntime','tensorflow','numpy','scipy','torch','tflite-runtime','piper-tts','ai-edge-litert','microwakeword','pymicro-features']
if '--one' in sys.argv:
    distributions = {d.metadata['Name'].lower().replace('_','-'): d.version for d in md.distributions()}
    print(json.dumps({'executable':sys.executable,'version':sys.version,'packages':{n:distributions.get(n) for n in NAMES}}))
else:
    rows=[]
    for name in ['python3.11','python3.12','python3.13','python3.14']:
        executable=shutil.which(name)
        if executable:
            result=subprocess.run([executable,str(Path(__file__).resolve()),'--one'],capture_output=True,text=True)
            rows.append({'command':name,'exit_code':result.returncode,'result':json.loads(result.stdout) if result.returncode==0 else result.stderr})
    usage=shutil.disk_usage('.')
    print(json.dumps({'platform':platform.platform(),'machine':platform.machine(),'runtimes':rows,'disk_free_bytes':usage.free,'memory_bytes':None,'memory_probe':'sysctl hw.memsize blocked by sandbox; no escalation','scope':'visible command-line interpreters only; no hidden venv/cache claim'},indent=2))
