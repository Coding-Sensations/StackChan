import pathlib,sys,unittest
from types import SimpleNamespace as NS
sys.path.insert(0,str(pathlib.Path(__file__).parents[1]/'streaming'))
from camera_authority_ledger import LocalCameraAuthorityLedger
from camera_host_dispatcher import ExistingSessionCameraDispatcher
from clock_mapping import ClockOwner
from camera_stop_dispatcher import CameraStartCustodian
class Session:
    device_id='device'
    conversation=NS(id='conversation')
    def __init__(self): self.turn=NS(id='turn',generation=1); self.active=True
    def _authorized(self):
        if not self.active:raise PermissionError()
    def _active(self):return self.active
class Tests(unittest.TestCase):
    def setUp(self):
        self.now=10.;self.s=Session();self.owner=ClockOwner('device','conversation','turn',1,1,1,'camera')
        self.ledger=LocalCameraAuthorityLedger(ui_confirmation=lambda *a:True,device_confirmation=lambda *a:True,clock=lambda:self.now,token=lambda:'a'*64)
    def issue(self,l=None):return (l or self.ledger).issue_local(self.s,self.owner,'webcam',7,'b'*32,'req',10)
    def consume(self):return self.ledger.consume_authority(self.s,self.issue(),'webcam','req')
    def test_absent_defaults_deny(self):
        with self.assertRaises(PermissionError):self.issue(LocalCameraAuthorityLedger(clock=lambda:self.now))
    def test_exact_session_replay_and_retention(self):
        token=self.issue()
        with self.assertRaises(PermissionError):self.ledger.consume_authority(Session(),token,'webcam','req')
        a=self.ledger.consume_authority(self.s,token,'webcam','req')
        with self.assertRaises(PermissionError):self.ledger.consume_authority(self.s,token,'webcam','req')
        self.assertIs(self.ledger.current_authority(self.s,a),a)
        self.now=21
        self.assertIsNone(self.ledger.current_authority(self.s,a))
        self.assertIs(self.ledger.retained_authority(self.s),a)
        with self.assertRaises(PermissionError):self.issue()
        self.assertFalse(self.ledger.release_after_checked_stop(self.s,a,object()))
    def test_turn_and_clock_fault_revoke(self):
        a=self.consume();self.s.turn.generation=2
        self.assertIsNone(self.ledger.current_authority(self.s,a))
        self.now=9
        with self.assertRaises(PermissionError):self.ledger.current_authority(self.s,a)
        self.assertIs(self.ledger.retained_authority(self.s),a)
    def test_wrong_mode_burns_token(self):
        t=self.issue()
        with self.assertRaises(PermissionError):self.ledger.consume_authority(self.s,t,'gestures','req')
        with self.assertRaises(PermissionError):self.ledger.consume_authority(self.s,t,'webcam','req')
    def test_actual_dispatcher_off_and_no_physical_proof(self):
        t=self.issue();d=ExistingSessionCameraDispatcher(self.s,CameraStartCustodian(),backend=self.ledger,clock=lambda:self.now)
        r=d.handle_local_request(dict(type='camera_permission_request',version=1,request_id='req',mode='webcam',ttl_seconds=10,opaque_authority=t))
        self.assertTrue(r['permission_current']);self.assertFalse(r['enabled']);self.assertFalse(r['capture_ready'])
        a=d.authority;self.assertIsNone(self.ledger.clock_receipt(None));self.assertIsNone(self.ledger.capture_receipt(None))
        d.handle_local_request(dict(type='camera_permission_off',version=1,request_id='off'))
        self.assertIsNone(self.ledger.current_authority(self.s,a));self.assertIs(self.ledger.retained_authority(self.s),a)
    def test_verified_cleanup_keeps_generation_floor(self):
        a=self.consume();self.ledger.revoke_authority(self.s,a)
        receipt=object();self.ledger.cleanup_confirmation=lambda s,aa,r:r is receipt
        self.assertFalse(self.ledger.release_after_checked_stop(self.s,a,object()))
        self.assertTrue(self.ledger.release_after_checked_stop(self.s,a,receipt))
        with self.assertRaises(PermissionError):self.issue()
    def test_actual_stop_custody_blocks_consumption(self):
        t=self.issue();custodian=CameraStartCustodian();custodian.reserve(self.s);custodian.retain(self.s)
        d=ExistingSessionCameraDispatcher(self.s,custodian,backend=self.ledger,clock=lambda:self.now)
        r=d.handle_local_request(dict(type='camera_permission_request',version=1,request_id='req',mode='webcam',ttl_seconds=10,opaque_authority=t))
        self.assertFalse(r['permission_current']);self.assertTrue(custodian.pending(self.s.device_id))
        self.assertIsNone(d.authority)
    def test_board_id_replay_after_cleanup(self):
        a=self.consume();self.ledger.revoke_authority(self.s,a)
        self.ledger.cleanup_confirmation=lambda *a:True
        self.assertTrue(self.ledger.release_after_checked_stop(self.s,a,object()))
        self.owner=ClockOwner('device','conversation','turn',1,2,1,'camera2')
        with self.assertRaises(PermissionError):self.issue()
    def test_reconnect_never_inherits_grant(self):
        a=self.consume();replacement=Session()
        self.assertIsNone(self.ledger.current_authority(replacement,a))
        self.assertIsNone(self.ledger.retained_authority(replacement))
        self.assertIs(self.ledger.retained_authority(self.s),a)
    def test_concurrent_single_consumption(self):
        from concurrent.futures import ThreadPoolExecutor
        t=self.issue()
        def consume():
            try:return self.ledger.consume_authority(self.s,t,'webcam','req')
            except PermissionError:return None
        with ThreadPoolExecutor(max_workers=8) as pool:
            results=list(pool.map(lambda _:consume(),range(8)))
        self.assertEqual(sum(r is not None for r in results),1)
    def test_postconsume_rejections_revoke_exact_ledger_authority(self):
        for host_clock,ttl in ((lambda:10.,1),(lambda:float('nan'),10),(lambda:30.,10)):
            with self.subTest(ttl=ttl):
                ledger=LocalCameraAuthorityLedger(ui_confirmation=lambda *a:True,
                    device_confirmation=lambda *a:True,clock=lambda:10.,token=lambda:'a'*64)
                t=self.issue(ledger)
                d=ExistingSessionCameraDispatcher(self.s,CameraStartCustodian(),backend=ledger,clock=host_clock)
                r=d.handle_local_request(dict(type='camera_permission_request',version=1,
                    request_id='req',mode='webcam',ttl_seconds=ttl,opaque_authority=t))
                self.assertFalse(r['permission_current']);self.assertIsNone(d.authority)
                a=ledger.retained_authority(self.s)
                self.assertIsNotNone(a)
                self.assertIsNone(ledger.current_authority(self.s,a))
                # Synthetic qualifier verifies exact receipt; rejection revokes but retains cleanup.
                receipt=object();ledger.cleanup_confirmation=lambda s,aa,rr:aa is a and rr is receipt
                self.assertFalse(ledger.release_after_checked_stop(self.s,a,object()))
                self.assertTrue(ledger.release_after_checked_stop(self.s,a,receipt))
    def test_off_revokes_issued_unconsumed_exact_session_token(self):
        t=self.issue();a=self.ledger.retained_authority(self.s)
        replacement=Session();self.ledger.revoke_session_authority(replacement)
        # A replacement session cannot cancel this exact owner's outstanding purpose.
        self.assertFalse(self.ledger._records[self.s.device_id].revoked)
        d=ExistingSessionCameraDispatcher(self.s,CameraStartCustodian(),backend=self.ledger,clock=lambda:self.now)
        d.handle_local_request(dict(type='camera_permission_off',version=1,request_id='off'))
        r=d.handle_local_request(dict(type='camera_permission_request',version=1,request_id='req',
            mode='webcam',ttl_seconds=10,opaque_authority=t))
        self.assertFalse(r['permission_current']);self.assertIsNone(d.authority)
        self.assertIs(self.ledger.retained_authority(self.s),a)
        receipt=object();self.ledger.cleanup_confirmation=lambda ss,aa,rr:ss is self.s and aa is a and rr is receipt
        self.assertTrue(self.ledger.release_after_checked_stop(self.s,a,receipt))
    def test_false_or_exception_confirmation(self):
        self.ledger.device_confirmation=lambda *a:False
        with self.assertRaises(PermissionError):self.issue()
        def fail(*a):raise RuntimeError('synthetic fault')
        self.ledger.device_confirmation=fail
        with self.assertRaises(RuntimeError):self.issue()
        self.assertIsNone(self.ledger.retained_authority(self.s))
if __name__=='__main__':unittest.main()
