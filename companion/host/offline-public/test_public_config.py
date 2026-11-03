"""Synthetic trust pins only; no certificate creation or listeners."""
import importlib.util
import os
from pathlib import Path
import json
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1] / 'streaming/public_config.py'

def module():
    spec=importlib.util.spec_from_file_location('fixture_config', MODULE)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result)
    return result

class PublicConfigChecks(unittest.TestCase):
    def setUp(self):
        self.previous=os.environ.pop('STACKCHAN_BINDING_CONFIG',None)
    def tearDown(self):
        os.environ.pop('STACKCHAN_BINDING_CONFIG',None)
        if self.previous is not None:os.environ['STACKCHAN_BINDING_CONFIG']=self.previous
    def test_unconfigured_is_unavailable(self):
        config=module()
        self.assertEqual((config.DEVICE,config.HOST,config.EXPIRY),('','',0))
        with self.assertRaises(ValueError):config.require()
    def test_owner_only_fixture_and_exact_endpoint(self):
        fixture=dict(device='stackchan-001122334455',host='192.168.42.10',port=18443,uri='wss://192.168.42.10:18443/companion/audio',ca_sha256='a'*64,expiry=2000000000,cert_sha256='b'*64,cert_begin=1900000000,cert_end=2000000000,native_library_sha256='c'*64)
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'bindings.json';path.write_text(json.dumps(fixture));path.chmod(0o600)
            os.environ['STACKCHAN_BINDING_CONFIG']=str(path);config=module();config.require()
            self.assertEqual(config.CA_HASH,'a'*64)
            fixture['uri']='wss://192.168.42.11:18443/companion/audio';path.write_text(json.dumps(fixture))
            with self.assertRaises(ValueError):module().require()
            fixture['uri']='wss://192.168.42.10:18443/companion/audio';path.write_text(json.dumps(fixture));path.chmod(0o644)
            with self.assertRaises(ValueError):module().require()
    def test_directional_selection_is_explicit_and_owner_only(self):
        fixture=dict(device='stackchan-001122334455',host='192.168.42.10',port=18443,uri='wss://192.168.42.10:18443/companion/audio',ca_sha256='a'*64,expiry=2000000000,cert_sha256='b'*64,cert_begin=1900000000,cert_end=2000000000,native_library_sha256='c'*64)
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'bindings.json';path.write_text(json.dumps(fixture));path.chmod(0o600);os.environ['STACKCHAN_BINDING_CONFIG']=str(path)
            config=module();config.require();self.assertEqual(config.NATIVE_AUDIO_BACKEND,'legacy_bridge_v1')
            selected=dict(fixture,native_audio_backend='directional_hal_v2',native_library_path='/synthetic/candidate.dylib')
            path.write_text(json.dumps(selected));config=module();config.require();self.assertEqual(config.NATIVE_AUDIO_BACKEND,'directional_hal_v2');self.assertEqual(config.LIBRARY,Path('/synthetic/candidate.dylib'))
            # Syntactic selection never reads/loads the missing library or opens IO.
            for changed in [dict(selected,native_audio_backend='legacy_bridge_v1'),dict(selected,native_audio_backend='unknown'),dict(selected,native_library_path='relative.dylib'),dict(selected,native_library_path='/a/../b.dylib'),dict(selected,native_library_path='/a\nb.dylib'),dict(selected,native_library_sha256='invalid'),dict(fixture,native_audio_backend='directional_hal_v2'),dict(fixture,native_library_path='/synthetic/lib.dylib')]:
                path.write_text(json.dumps(changed))
                with self.assertRaises(ValueError):module().require()
            for duration in (0,16,True,'10'):
                path.write_text(json.dumps(dict(selected,native_audio_test_seconds=duration)))
                with self.assertRaises(ValueError):module().require()
            path.write_text(json.dumps(dict(selected,native_audio_test_seconds=10)));config=module();config.require();self.assertEqual(config.NATIVE_AUDIO_TEST_SECONDS,10)
            path.write_text(json.dumps(selected));path.chmod(0o644)
            with self.assertRaises(ValueError):module().require()
    def test_duplicate_config_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'bindings.json';path.write_text('{"device":"one","device":"two"}');path.chmod(0o600)
            os.environ['STACKCHAN_BINDING_CONFIG']=str(path)
            with self.assertRaises(ValueError):module().require()

if __name__=='__main__':unittest.main()
