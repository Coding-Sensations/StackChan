"""Mode/UI/approval/provider selection offline; no listener or device IO."""
import json,time,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch,sentinel
import numeric_pairing_worker as worker
from native_conversation import NativeRoute
from home_lab.devices import DeviceRegistry
from remote_gateway import RemoteGateway

class ApprovalTests(unittest.TestCase):
    def setUp(self):
        tmp=tempfile.TemporaryDirectory(prefix='native-mode-fixture-');self.addCleanup(tmp.cleanup)
        self.root=Path(tmp.name);(self.root/'notes').mkdir();(self.root/'.tools').mkdir()
        p=patch.object(worker,'ROOT',self.root);p.start();self.addCleanup(p.stop)
        self.route=dict(session_reference='operator-confirmed-original-dot',call_confirmed=True,route_confirmed=True,forwarding_approved=True,duration_seconds=60)
    def authorization(self,mode):
        value=dict(approved=True,audio_allowed=True,valid_until_unix=time.time()+60,expected_boot_nonce=91,audio_mode=mode,native_conversation_allowed=mode=='native_conversation',native_route=self.route)
        (self.root/'notes/NUMERIC-PAIRING-RUNTIME-AUTHORIZATION.json').write_text(json.dumps(value))
    def test_native_gui_must_match_native_authorization(self):
        self.authorization('native_conversation');routes=[]
        self.assertEqual(worker._consume_approval(audio_enabled=True,native_route_sink=routes.append,requested_mode='native_conversation'),91)
        self.assertEqual(routes,[NativeRoute('operator-confirmed-original-dot',60)])
        self.assertTrue((self.root/'.tools/NUMERIC-PAIRING-AUDIO-CHECK-CONSUMED').exists())
    def test_mode_mismatch_denied_before_oneuse_or_route_snapshot(self):
        for requested,authorized in [('native_conversation','transport_test'),('transport_test','native_conversation')]:
            self.authorization(authorized);routes=[]
            with self.assertRaises(worker.PairingError):worker._consume_approval(audio_enabled=True,native_route_sink=routes.append,requested_mode=requested)
            self.assertEqual(routes,[]);self.assertFalse((self.root/'.tools/NUMERIC-PAIRING-AUDIO-CHECK-CONSUMED').exists())
    def test_missing_native_flag_cannot_select_discard_under_native_approval(self):
        self.authorization('native_conversation')
        with self.assertRaises(worker.PairingError):worker._consume_approval(audio_enabled=True,native_route_sink=lambda _:None)
        self.assertFalse((self.root/'.tools/NUMERIC-PAIRING-AUDIO-CHECK-CONSUMED').exists())
    def test_native_confirmation_failure_does_not_fallback(self):
        self.authorization('native_conversation');value=json.loads((self.root/'notes/NUMERIC-PAIRING-RUNTIME-AUTHORIZATION.json').read_text());value['native_route']['call_confirmed']=False
        (self.root/'notes/NUMERIC-PAIRING-RUNTIME-AUTHORIZATION.json').write_text(json.dumps(value))
        with self.assertRaises(worker.PairingError):worker._consume_approval(audio_enabled=True,native_route_sink=lambda _:None,requested_mode='native_conversation')
        self.assertFalse((self.root/'.tools/NUMERIC-PAIRING-AUDIO-CHECK-CONSUMED').exists())

class ProviderTests(unittest.TestCase):
    def test_native_provider_factory_cannot_use_cue_or_discard(self):
        route=NativeRoute('original-dot',60);args=SimpleNamespace(audio_mode='native_conversation')
        with patch('transport_test.TransportGateway',side_effect=AssertionError('discard provider selected')),patch('native_conversation.make_provider',return_value=sentinel.native) as factory:
            gateway=worker._audio_transport(args,DeviceRegistry(),route)
            self.assertIsInstance(gateway,RemoteGateway)
            self.assertIs(gateway.provider_factory('synthetic-device'),sentinel.native)
            factory.assert_called_once_with(route)
    def test_native_mode_without_route_rejected_before_TLS_or_provider(self):
        with patch.object(worker.ssl,'SSLContext',side_effect=AssertionError('TLS/key IO attempted')):
            with self.assertRaises(worker.PairingError):worker._runtime(SimpleNamespace(audio_mode='native_conversation'))
    def test_transport_mode_with_native_route_is_rejected(self):
        with self.assertRaises(worker.PairingError):worker._audio_transport(SimpleNamespace(audio_mode='transport_test'),DeviceRegistry(),NativeRoute('dot',60))

if __name__=='__main__':unittest.main()
