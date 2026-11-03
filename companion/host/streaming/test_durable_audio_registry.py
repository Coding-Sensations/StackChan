import unittest,tempfile
from pathlib import Path
from home_lab.storage import SQLiteStateStore
from home_lab.devices import DeviceRegistry
from durable_audio_registry import DurableAudioRegistry
from gateway_admission import authorize_audio_request

class Tests(unittest.TestCase):
    def test_cross_process_authority_view_observes_revocation(self):
        with tempfile.TemporaryDirectory(prefix='stackchan-fixture-') as tmp:
            a=SQLiteStateStore(Path(tmp)/'state');b=SQLiteStateStore(Path(tmp)/'state')
            try:
                owner=DeviceRegistry(a);side=DurableAudioRegistry(b)
                device,token=owner.redeem_pairing_code(owner.create_pairing_code(),device_id='synthetic',label='Synthetic',capabilities=('microphone','speaker'))
                self.assertEqual(side.authenticate(token),device.id)
                headers=[('Authorization','Bearer '+token),('X-Companion-Device',device.id)]
                self.assertEqual(authorize_audio_request(side,'/companion/audio',headers).device_id,device.id)
                owner.revoke(device.id)
                self.assertIsNone(side.authenticate(token))
                with self.assertRaises(PermissionError):authorize_audio_request(side,'/companion/audio',headers)
            finally:a.close();b.close()
    def test_read_only_view_has_no_issue_or_revoke_surface(self):
        with tempfile.TemporaryDirectory(prefix='stackchan-fixture-') as tmp:
            store=SQLiteStateStore(Path(tmp)/'state')
            try:
                view=DurableAudioRegistry(store)
                self.assertIsNone(view.authenticate('invalid-synthetic'));self.assertIsNone(view.get('absent'))
                self.assertFalse(hasattr(view,'create_pairing_code'));self.assertFalse(hasattr(view,'revoke'))
            finally:store.close()
if __name__=='__main__':unittest.main()
