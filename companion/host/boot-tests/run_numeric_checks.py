#!/usr/bin/env python3
"""Compile actual numeric pairing parser with deterministic transport boundaries."""
from pathlib import Path
import json, os, subprocess, tempfile, hashlib
repo=Path(__file__).resolve().parents[3]
tests=Path(__file__).resolve().parent
sdk=Path(os.environ['IDF_PATH'])
cjson=sdk/'components/json/cJSON'
with tempfile.TemporaryDirectory(prefix='stackchan-numeric-tests-') as directory:
    output=Path(directory)
    flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    subprocess.run([os.environ.get('CC','clang'),'-c',*flags,str(cjson/'cJSON.c'),'-o',str(output/'cJSON.o')],check=True)
    for name in ['numeric_pairing','numeric_tls','wireless_pairing_next','remembered_numeric']:
        argv=[os.environ.get('CXX','clang++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-constant-logical-operand',*flags,'-I'+str(repo/'firmware/main'),'-I'+str(repo/'firmware/main/companion'),'-I'+str(tests/'numeric-fixture'),'-I'+str(cjson)]
        argv += ['-DSTACKCHAN_PIN_'+k+'='+json.dumps(v) for k,v in dict(DEVICE='stackchan-001122334455',HOST='192.168.42.10',URI='wss://192.168.42.10:18443/companion/audio',CA_SHA256=hashlib.sha256(b'synthetic-CA-no-authority').hexdigest(),EXPIRY=2000000000).items()]
        if name in ('numeric_tls','wireless_pairing_next','remembered_numeric'):argv+=['-include',str(tests/'numeric-fixture/sdk_boundary.hpp'),str(repo/'firmware/main/companion/numeric_pairing.cc'),str(output/'cJSON.o')]
        argv+=[str(tests/('test_'+name+'.cpp')),'-o',str(output/name)]
        subprocess.run(argv,check=True)
        subprocess.run([str(output/name)],check=True)
print(json.dumps({'result':'passed','groups':4,'sanitizers':['address','undefined'],'hardware':False,'network':False}))
