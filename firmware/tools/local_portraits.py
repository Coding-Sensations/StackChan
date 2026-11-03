"""Validate an explicitly selected external local adapter. Never fetches or copies artwork."""
from pathlib import Path
import argparse,hashlib,json,re

def resolve(enabled,manifest,source):
 if not enabled:
  if manifest:raise ValueError('Local portraits disabled; remove manifest or explicitly enable local inputs')
  return {'enabled':False}
 if not manifest:raise ValueError('Explicit local portrait manifest required')
 path=Path(manifest)
 if not path.is_absolute() or not path.is_file() or path.is_symlink():raise ValueError('Absolute regular manifest required')
 if path.stat().st_size>8192:raise ValueError('Local manifest exceeds bound')
 def pairs(rows):
  out={}
  for key,value in rows:
   if key in out:raise ValueError('Duplicate manifest field')
   out[key]=value
  return out
 try:data=json.loads(path.read_text(),object_pairs_hook=pairs)
 except (OSError,ValueError) as e:raise ValueError('Invalid local portrait manifest') from e
 required={'schema','header','sha256','providers','provenance','rights_status'}
 if not isinstance(data,dict) or set(data)!=required or type(data['schema']) is not int or data['schema']!=1:raise ValueError('Unsupported local portrait manifest schema')
 if not isinstance(data['header'],str) or any(c in data['header'] for c in '\n\r";\\'):raise ValueError('Invalid header path')
 header=Path(data['header'])
 if not header.is_absolute() or not header.is_file() or header.is_symlink() or header.resolve().is_relative_to(Path(source).resolve()):raise ValueError('Header must be an explicit external regular local file')
 if not isinstance(data['sha256'],str) or not re.fullmatch('[a-f0-9]{64}',data['sha256']):raise ValueError('Pinned header SHA256 required')
 if not isinstance(data['providers'],list) or not data['providers'] or any(not isinstance(x,str) for x in data['providers']) or len(set(data['providers']))!=len(data['providers']) or any(x not in ('hermes','muse','claude','codex') for x in data['providers']):raise ValueError('Exact optional provider identities required')
 if not isinstance(data['provenance'],str) or not 1<=len(data['provenance'])<=2000 or data['rights_status'] not in ('unverified-local-input','permission-documented'):raise ValueError('Provenance and explicit rights status required; this is not license verification')
 if header.stat().st_size>262144:raise ValueError('Local adapter exceeds bound')
 actual=hashlib.sha256(header.read_bytes()).hexdigest()
 if actual!=data['sha256']:raise ValueError('Local adapter SHA256 mismatch')
 return {'enabled':True,**data,'header':str(header.resolve()),'manifest_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'license_clearance_claimed':False,'private_local_input':True}

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--enabled',choices=('ON','OFF'),required=True);p.add_argument('--manifest');p.add_argument('--source',required=True);p.add_argument('--output',required=True);p.add_argument('--cmake',required=True);a=p.parse_args()
 try:value=resolve(a.enabled=='ON',a.manifest or None,a.source)
 except ValueError as e:p.error(str(e))
 output=Path(a.output);output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(value,indent=2)+'\n');text='set(STACKCHAN_LOCAL_PORTRAITS '+('1' if value['enabled'] else '0')+')\n'
 for provider in ('hermes','muse','claude','codex'):text+='set(STACKCHAN_LOCAL_'+provider.upper()+'_PORTRAIT '+('1' if value['enabled'] and provider in value['providers'] else '0')+')\n'
 if value['enabled']:text+='set(STACKCHAN_VALIDATED_LOCAL_PORTRAIT_HEADER "'+value['header']+'")\n'
 Path(a.cmake).write_text(text)
