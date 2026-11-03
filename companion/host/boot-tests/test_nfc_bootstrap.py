import importlib.util,tempfile,tarfile,io,json,hashlib
from pathlib import Path
from unittest.mock import patch
p=Path(__file__).resolve().parents[2] / 'setup_native_nfc.py'
s=importlib.util.spec_from_file_location('bootstrap',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
name='M5HAL';sha='1'*40;b=io.BytesIO();license=b'synthetic MIT fixture'
with tarfile.open(fileobj=b,mode='w:gz') as t:
 for path,data in [('LICENSE',license),('src/test.cpp',b'fixture')]:
  v=tarfile.TarInfo(name+'-'+sha+'/'+path);v.size=len(data);t.addfile(v,io.BytesIO(data))
a=b.getvalue();pin={'repo':'m5stack/'+name,'sha':sha,'archive_url':'https://codeload.github.com/m5stack/'+name+'/tar.gz/'+sha,'archive_bytes':len(a),'archive_sha256':hashlib.sha256(a).hexdigest(),'license_sha256':hashlib.sha256(license).hexdigest()}
with tempfile.TemporaryDirectory() as d:
 r=Path(d);(r/'companion').mkdir();(r/'companion/NFC-VENDOR-PINS.json').write_text(json.dumps({name:pin}))
 with patch.object(m.urllib.request,'urlopen',side_effect=lambda *a,**k:io.BytesIO(b.getvalue())):
  m.setup(r);m.setup(r)
  extra=r/'firmware/components/M5HAL/src/unpinned.cpp';extra.write_text('extra')
  try:m.setup(r);raise AssertionError('extra accepted')
  except ValueError as e:assert 'file set mismatch' in str(e)
  extra.unlink();link=extra.parent/'linked';link.symlink_to('/tmp',target_is_directory=True)
  try:m.setup(r);raise AssertionError('symlink accepted')
  except ValueError as e:assert 'symlink refused' in str(e)
  link.unlink();pin['archive_sha256']='0'*64;(r/'companion/NFC-VENDOR-PINS.json').write_text(json.dumps({name:pin}))
  try:m.setup(r);raise AssertionError('hash mismatch accepted')
  except ValueError as e:assert 'Archive verification failed' in str(e)
print('PASS bootstrap clean install, existing verify, extra-source/symlink/hash rejection')
