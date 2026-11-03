import unittest
from home_lab.devices import DeviceRegistry
from gateway_admission import authorize_audio_request

class Tests(unittest.TestCase):
    def setUp(self):
        self.registry=DeviceRegistry()
        code=self.registry.create_pairing_code()
        self.device,self.token=self.registry.redeem_pairing_code(code,device_id='synthetic-admission',label='Synthetic fixture',capabilities=('microphone','speaker'))
        self.headers=[('Authorization','Bearer '+self.token),('X-Companion-Device',self.device.id)]
    def deny(self,headers=None,path='/companion/audio'):
        with self.assertRaises(PermissionError) as e:
            authorize_audio_request(self.registry,path,self.headers if headers is None else headers)
        self.assertEqual(str(e.exception),'audio admission denied')
        self.assertNotIn(self.token,str(e.exception))
    def test_authenticated_existing_grant_and_redacted_repr(self):
        grant=authorize_audio_request(self.registry,'/companion/audio',self.headers)
        self.assertEqual(grant.device_id,self.device.id);self.assertEqual(grant.credential,self.token)
        self.assertNotIn(self.token,repr(grant));self.assertNotIn(self.device.id,repr(grant))
    def test_duplicate_case_insensitive_authority_headers(self):
        self.deny(self.headers+[('authorization','Bearer '+self.token)])
        self.deny(self.headers+[('x-companion-device',self.device.id)])
    def test_wrong_device_or_invalid_grant(self):
        self.deny([self.headers[0],('X-Companion-Device','other-device')])
        self.deny([('Authorization','Bearer invalid-synthetic'),self.headers[1]])
    def test_revoked_device_denied(self):
        self.registry.revoke(self.device.id);self.deny()
    def test_missing_audio_capability_denied(self):
        code=self.registry.create_pairing_code()
        device,token=self.registry.redeem_pairing_code(code,device_id='synthetic-no-speaker',label='Synthetic fixture',capabilities=('microphone',))
        self.deny([('Authorization','Bearer '+token),('X-Companion-Device',device.id)])
    def test_path_fixture_and_header_bounds(self):
        self.deny(path='/companion/audio?token=synthetic');self.deny(path='/synthetic')
        self.deny(self.headers+[('X-StackChan-Fixture','synthetic-only')])
        self.deny(self.headers+[('X-Padding','x'*8192)])
        self.deny([('X-Padding','x')]*65)
    def test_control_characters_missing_and_overlong_credentials(self):
        self.deny(self.headers+[('X-Other','x\r\ny')]);self.deny([])
        self.deny([('Authorization','Bearer '+'x'*257),self.headers[1]])
        self.deny([('Authorization','Bearer synthetic token'),self.headers[1]])
if __name__=='__main__':unittest.main()
