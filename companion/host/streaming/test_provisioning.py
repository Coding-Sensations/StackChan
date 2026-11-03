import tempfile
import unittest
from pathlib import Path
from home_lab.devices import DeviceRegistry
from home_lab.storage import SQLiteStateStore
from provisioning import pack_grant, unpack_grant, ProvisioningBroker
from gateway_admission import authorize_audio_request

URL='wss://synthetic.example/companion/audio'
class Tests(unittest.TestCase):
    def test_frame_rejects_truncation_trailing_and_bad_endpoint(self):
        frame=pack_grant(URL,'synthetic-provisioning','fixture-token','fixture-request')
        self.assertEqual(unpack_grant(frame),(URL,'synthetic-provisioning','fixture-token','fixture-request'))
        for n in range(len(frame)):
            with self.assertRaises(ValueError):unpack_grant(frame[:n])
        for bad in [frame+b'x',b'XXXX'+frame[4:],frame[:4]+b'\x02'+frame[5:]]:
            with self.assertRaises(ValueError):unpack_grant(bad)
        for endpoint in ['ws://host/companion/audio',URL+'?token=fixture','wss://user:fixture@host/companion/audio']:
            with self.assertRaises(ValueError):pack_grant(endpoint,'d','c','r')
    def test_owner_and_route_gate_before_code_consumption(self):
        registry=DeviceRegistry();broker=ProvisioningBroker(registry,URL);code=registry.create_pairing_code()
        args=dict(device_id='synthetic-provisioning',label='Synthetic',request_id='fixture-request')
        for a,b in [(False,False),(True,False),(False,True)]:
            with self.assertRaises(PermissionError):broker.redeem(code,**args,owner_authorized=a,route_qualified=b)
        result=broker.redeem(code,**args,owner_authorized=True,route_qualified=True)
        uri,device,credential,request=unpack_grant(result.frame)
        self.assertNotIn(credential,repr(result));self.assertEqual(registry.authenticate(credential),device)
        self.assertEqual(authorize_audio_request(registry,'/companion/audio',[('Authorization','Bearer '+credential),('X-Companion-Device',device)]).device_id,device)
        with self.assertRaises(ValueError):broker.redeem(code,**{**args,'device_id':'another-synthetic'},owner_authorized=True,route_qualified=True)
    def test_durable_hash_restart_and_revocation(self):
        with tempfile.TemporaryDirectory(prefix='stackchan-fixture-') as temp:
            store=SQLiteStateStore(Path(temp)/'state');registry=DeviceRegistry(store)
            code=registry.create_pairing_code();broker=ProvisioningBroker(registry,URL)
            result=broker.redeem(code,device_id='synthetic-provisioning',label='Synthetic',request_id='fixture-request',owner_authorized=True,route_qualified=True)
            token=unpack_grant(result.frame)[2];store.close()
            store=SQLiteStateStore(Path(temp)/'state');registry=DeviceRegistry(store)
            self.assertEqual(registry.authenticate(token),'synthetic-provisioning')
            self.assertNotIn(token.encode(),store.path.read_bytes());registry.revoke('synthetic-provisioning');store.close()
            store=SQLiteStateStore(Path(temp)/'state');registry=DeviceRegistry(store)
            self.assertIsNone(registry.authenticate(token));store.close()
    def test_invalid_fields_do_not_consume_code(self):
        registry=DeviceRegistry();broker=ProvisioningBroker(registry,URL);code=registry.create_pairing_code()
        with self.assertRaises(ValueError):broker.redeem(code,device_id='synthetic',label='Synthetic',request_id='x'*129,owner_authorized=True,route_qualified=True)
        result=broker.redeem(code,device_id='synthetic',label='Synthetic',request_id='fixture',owner_authorized=True,route_qualified=True)
        self.assertEqual(unpack_grant(result.frame)[1],'synthetic')
if __name__=='__main__':unittest.main()
