"""Pure synthetic tests. No sockets, hardware, codecs or capture invoked."""
import sys,pathlib,unittest
from unittest.mock import patch
import os
root=pathlib.Path(__file__).resolve().parents[3]
home_lab=os.environ.get('HOME_LAB_ROOT')
if not home_lab:raise RuntimeError('HOME_LAB_ROOT is required for offline session fixtures')
sys.path[:0]=[str(pathlib.Path(home_lab)/'src'),str(root/'bridge/host-adapter'),str(pathlib.Path(__file__).resolve().parent)]
import test_camera_wire as test_wire
from camera_permission_wire import PairedSessionCameraPermissionDispatcher
from camera_stop_dispatcher import CameraStartCustodian
class Lifecycle(test_wire.Tests):
    def test_fault_after_consumption_keeps_context_and_queues_stop(self):
        self.prepare();p=self.d.pop_control()
        original=self.d.backend.consume_authority
        def fault(*args,**kwargs):
            result=original(*args,**kwargs)
            raise RuntimeError('synthetic post-consumption fault')
        with patch.object(self.d.backend,'consume_authority',fault):self.d.handle_control(self.reply(p,True))
        self.assertIsNone(self.d.authority)
        self.assertIsNotNone(self.d.backend.retained_authority(self.s))
        context=self.d._context
        self.assertIsNotNone(context)
        self.assertTrue(self.d.custodian.barrier_held(context))
        self.assertTrue(self.d.is_current_cleanup(context))
        self.assertEqual(self.s.camera_stop_dispatcher.pop_control()['type'],'camera_stop_challenge')
        self.assertEqual(self.d.pop_control()['type'],'camera_permission_cancel')
    def test_issue_fault_before_consumption_still_has_exact_stop_context(self):
        self.prepare();p=self.d.pop_control()
        with patch.object(self.d.backend,'issue_local',side_effect=RuntimeError('synthetic issue failure')):
            self.d.handle_control(self.reply(p,True))
        self.assertIsNone(self.d.backend.retained_authority(self.s))
        context=self.d._context
        self.assertIsNotNone(context);self.assertTrue(self.d.custodian.barrier_held(context))
        self.assertTrue(self.d.is_current_cleanup(context));self.assertFalse(self.d.readiness()['enabled'])
        challenge=self.s.camera_stop_dispatcher.pop_control()
        self.assertEqual(challenge['authority_id'],'7');self.assertEqual(challenge['boot'],'b'*32)
    def test_atomic_custody_failure_happens_before_issuer(self):
        self.prepare();p=self.d.pop_control()
        with patch.object(self.d.custodian,'reserve_context',side_effect=PermissionError('synthetic admission denied')):
            with patch.object(self.d.backend,'issue_local',side_effect=AssertionError('issuer must not run')) as issue:
                self.d.handle_control(self.reply(p,True));issue.assert_not_called()
        self.assertIsNone(self.d.backend.retained_authority(self.s));self.assertIsNotNone(self.d._context)
        self.assertFalse(self.d.custodian.pending(self.s.device_id))
    def test_fault_after_reservation_keeps_published_cleanup_context(self):
        self.prepare();p=self.d.pop_control();original=self.d.custodian.reserve_context
        def reserve(*args):
            original(*args);raise RuntimeError('synthetic after-reservation failure')
        with patch.object(self.d.custodian,'reserve_context',reserve):self.d.handle_control(self.reply(p,True))
        self.assertIsNone(self.d.backend.retained_authority(self.s))
        self.assertTrue(self.d.custodian.barrier_held(self.d._context))
        self.assertTrue(self.d.is_current_cleanup(self.d._context))
        self.assertEqual(self.s.camera_stop_dispatcher.pop_control()['type'],'camera_stop_challenge')
    def test_commit_enqueue_failure_retains_exact_owner(self):
        self.prepare();p=self.d.pop_control();original=self.d._enqueue
        def enqueue(v):
            if v['type']=='camera_permission_commit':raise BufferError('synthetic transport backpressure')
            return original(v)
        with patch.object(self.d,'_enqueue',enqueue):self.d.handle_control(self.reply(p,True))
        self.assertIsNone(self.d.authority);self.assertIsNotNone(self.d.backend.retained_authority(self.s))
        self.assertTrue(self.d.custodian.barrier_held(self.d._context))
        self.assertEqual(self.s.camera_stop_dispatcher.pop_control()['type'],'camera_stop_challenge')
    def test_precommit_disconnect_retains_without_grant(self):
        self.prepare();p=self.d.pop_control()
        with patch.object(self.d.backend,'issue_local',side_effect=RuntimeError('synthetic issue failure')):
            self.d.handle_control(self.reply(p,True))
        c=self.d._context;self.d.connection_lost()
        self.assertIs(self.d._context,c);self.assertTrue(self.d.custodian.barrier_held(c))
        self.assertIsNotNone(self.s.camera_stop_dispatcher.pop_control())
    def test_peer_ack_cannot_release_precommit_cleanup(self):
        self.prepare();p=self.d.pop_control()
        with patch.object(self.d.backend,'issue_local',side_effect=RuntimeError('synthetic issue failure')):
            self.d.handle_control(self.reply(p,True))
        stop=self.s.camera_stop_dispatcher;c=self.d._context
        stop.handle_control({'type':'camera_stop_acknowledged','version':1})
        self.assertFalse(stop.completed);self.assertTrue(self.d.custodian.barrier_held(c))
class MediaReservation(unittest.TestCase):
    setUp=test_wire.Tests.setUp
    prepare=test_wire.Tests.prepare
    reply=test_wire.Tests.reply
    def test_exact_active_reservation_adopts_without_second_reserve(self):
        self.prepare();p=self.d.pop_control();self.d.handle_control(self.reply(p,True))
        stop=self.s.camera_stop_dispatcher;context=self.d._context
        stop.reserve_for_media()
        from webcam_transport import WebcamPurpose
        b=context.binding
        purpose=WebcamPurpose(b.device,b.conversation,b.turn,b.generation,b.epoch)
        stop.bind_active(purpose)
        self.assertIs(stop.context,context)
        self.assertTrue(self.d.custodian.can_adopt_reserved(self.s,context,'webcam'))
        self.assertFalse(self.d.readiness()['capture_ready'])
    def test_gesture_permission_never_adopts_webcam_media(self):
        self.d.handle_local_request(dict(type='camera_permission_prepare',version=1,request_id='gesture-fixture',mode='gestures',ttl_seconds=10))
        p=self.d.pop_control();self.d.handle_control(self.reply(p,True))
        self.assertIsNotNone(self.d.authority)
        with self.assertRaises(PermissionError):self.s.camera_stop_dispatcher.reserve_for_media()
    def test_retained_cleanup_cannot_adopt_media_reservation(self):
        self.prepare();p=self.d.pop_control();self.d.handle_control(self.reply(p,True))
        self.d.handle_local_request(dict(type='camera_permission_off',version=1,request_id='off'))
        with self.assertRaises(PermissionError):self.s.camera_stop_dispatcher.reserve_for_media()
    def test_mismatched_media_purpose_cannot_bind(self):
        self.prepare();p=self.d.pop_control();self.d.handle_control(self.reply(p,True))
        from webcam_transport import WebcamPurpose
        b=self.d._context.binding
        purpose=WebcamPurpose(b.device,b.conversation,b.turn,b.generation+1,b.epoch)
        with self.assertRaises(PermissionError):self.s.camera_stop_dispatcher.bind_active(purpose)
if __name__=='__main__':unittest.main()
