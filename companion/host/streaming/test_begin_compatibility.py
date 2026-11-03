"""Production opening handshake only; synthetic grant, no media or hardware."""
import unittest
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
from native_conversation import NativeConversationSession
from test_streaming import Bridge

class BeginCompatibility(unittest.TestCase):
    def setUp(self):
        self.registry=DeviceRegistry()
        device,token=self.registry.redeem_pairing_code(self.registry.create_pairing_code(),device_id='synthetic-begin-compatibility',label='Synthetic',capabilities=('microphone','speaker'))
        self.bridge=Bridge();self.arms=[]
        self.session=NativeConversationSession(self.registry,ConversationSessions(),self.bridge,token,device.id,self.arms.append,native_session_ref='synthetic-inert-route')
    def tearDown(self):self.session.close()
    def test_production_begin_carries_firmware_identity_contract(self):
        hello=self.session.control({'type':'begin','request_id':'synthetic-first-request'})
        self.assertIn('conversation_id',hello,'firmware rejects begin without conversation_id')
        self.assertEqual(hello['conversation_id'],self.session.conversation.id)
        self.assertEqual(hello['turn_id'],self.session.turn.id)
        self.assertIn('turn_generation',hello,'firmware requires a decimal string turn_generation')
        self.assertIs(type(hello['turn_generation']),str)
        self.assertEqual(hello['turn_generation'],str(self.session.turn.generation))
        self.assertRegex(hello['turn_generation'],r'^[1-9][0-9]{0,19}$')
        self.assertLessEqual(int(hello['turn_generation']),2**64-1)
        self.assertFalse(hello['fixture'])
        self.assertEqual(hello['audio'],{'uplink_rate':16000,'downlink_rate':24000,'channels':1,'frame_ms':60})
        self.assertEqual(self.arms,[])
    def test_ack_and_heartbeat_preserve_physical_consent_gate(self):
        hello=self.session.control({'type':'begin','request_id':'synthetic-first-request'})
        self.session.control({'type':'ack','epoch':hello['epoch']})
        self.session.control({'type':'heartbeat','epoch':hello['epoch']})
        self.assertTrue(self.session.acknowledged)
        self.assertFalse(self.session.armed)
        self.assertEqual(self.arms,[])
    def test_duplicate_begin_keeps_same_identity_without_arming(self):
        hello=self.session.control({'type':'begin','request_id':'synthetic-first-request'})
        self.assertIn('turn_generation',hello,'deduplicated begin must carry the firmware contract too')
        again=self.session.control({'type':'begin','request_id':'synthetic-first-request'})
        self.assertEqual(hello,again)
        self.assertEqual(self.arms,[])
    def test_next_turn_updates_decimal_generation_without_losing_conversation(self):
        first=self.session.control({'type':'begin','request_id':'synthetic-first-request'})
        second=self.session.control({'type':'begin','request_id':'synthetic-next-request'})
        self.assertIn('turn_generation',second,'every begin must carry the current decimal generation')
        self.assertEqual(second['conversation_id'],self.session.conversation.id)
        self.assertEqual(second['turn_generation'],str(self.session.turn.generation))
        self.assertNotEqual(second['turn_id'],first['turn_id'])
        self.assertEqual(self.arms,[])

if __name__=='__main__':unittest.main()
