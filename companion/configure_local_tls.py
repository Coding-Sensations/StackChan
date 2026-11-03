#!/usr/bin/env python3
"""Embed a reviewed public CA only; no keys, credentials, network or device IO."""
import argparse, hashlib, ipaddress, json, pathlib, re, subprocess
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent/"host"/"streaming"))
from public_config import _load

def generate(ca,host,output,binding_config="",binding_output=None):
    if bool(ca)!=bool(host):raise ValueError('CA and host must be configured together')
    pins=_load(binding_config) if binding_config else None
    if ca and (not pins or pins['host']!=host):raise ValueError('approved binding required')
    if pins and not ca:raise ValueError('CA required with bindings')
    pem=''
    if ca:
        address=ipaddress.IPv4Address(host)
        networks=[ipaddress.IPv4Network(n) for n in ('10.0.0.0/8','172.16.0.0/12','192.168.0.0/16')]
        if str(address)!=host or not any(address in n for n in networks):raise ValueError('canonical RFC1918 host required')
        pem=pathlib.Path(ca).read_text(encoding='ascii')
        if len(pem)>16384 or not re.fullmatch(r'-----BEGIN CERTIFICATE-----\s+[A-Za-z0-9+/=\r\n]+-----END CERTIFICATE-----\s*',pem):raise ValueError('one public PEM certificate required; keys are prohibited')
        parsed=subprocess.run(['openssl','x509','-in',str(ca),'-noout','-text'],capture_output=True,text=True,timeout=5)
        if parsed.returncode or 'CA:TRUE' not in parsed.stdout:raise ValueError('valid CA certificate required')
        valid=subprocess.run(['openssl','x509','-in',str(ca),'-noout','-checkend','0'],capture_output=True,text=True,timeout=5)
        if valid.returncode:raise ValueError('expired CA rejected')
    if pins and hashlib.sha256(pem.encode('ascii')).hexdigest()!=pins['ca_sha256']:raise ValueError('CA pin mismatch')
    if binding_output:
        values={'DEVICE':pins['device'] if pins else '', 'HOST':pins['host'] if pins else '', 'URI':pins['uri'] if pins else '', 'CA_SHA256':pins['ca_sha256'] if pins else '', 'EXPIRY':pins['expiry'] if pins else 0}
        header='#pragma once\n'+''.join('#define STACKCHAN_PIN_'+k+' '+json.dumps(v)+'\n' for k,v in values.items())
        pathlib.Path(binding_output).parent.mkdir(parents=True,exist_ok=True)
        pathlib.Path(binding_output).write_text(header)
    source='// Generated public trust; never contains server keys or grants.\n#include <cstdint>\n'
    source+='extern "C" const char* companion_local_ca_pem(){return R"SC_LOCAL_CA('+pem+')SC_LOCAL_CA";}\n'
    source+='extern "C" const char* companion_local_tls_host(){return '+json.dumps(host)+';}\n'
    source+='extern "C" uint32_t companion_local_pair_expiry_unix(){return '+str(pins['expiry'] if pins else 0)+'U;}\n'
    pathlib.Path(output).parent.mkdir(parents=True,exist_ok=True)
    pathlib.Path(output).write_text(source)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--ca',default='');p.add_argument('--host',default='');p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--binding-config',default='');p.add_argument('--binding-output',type=pathlib.Path)
    a=p.parse_args()
    try:generate(a.ca,a.host,a.output,a.binding_config,a.binding_output)
    except (ValueError,OSError,UnicodeError,subprocess.TimeoutExpired):p.exit(2,'Local TLS configuration rejected. No key material is accepted.\n')
