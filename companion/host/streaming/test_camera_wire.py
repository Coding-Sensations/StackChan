import sys,pathlib,unittest,json,asyncio
from types import SimpleNamespace as NS
import os
root=pathlib.Path(__file__).resolve().parents[3]
home_lab=os.environ.get('HOME_LAB_ROOT')
if not home_lab:raise RuntimeError('HOME_LAB_ROOT is required for offline session fixtures')
sys.path[:0]=[str(pathlib.Path(home_lab)/'src'),str(root/'bridge/host-adapter'),str(pathlib.Path(__file__).resolve().parent)]
from streaming_session import StreamingSession
from home_lab.sessions import ConversationSessions
from camera_permission_wire import PairedSessionCameraPermissionDispatcher
from camera_stop_dispatcher import CameraStartCustodian
from remembered_pairing_worker import RememberedWorker
from gateway_connection import run_connection
from native_conversation import HostAudioDeviceRoute,host_audio_device_gateway
class Registry:
    def authenticate(self,c):return 'synthetic-device' if c=='synthetic-fixture' else None
    def get(self,d):return NS(status='active',capabilities=['microphone','speaker'])
class Bridge:
    adapter=NS(playing=False,set_playing=lambda epoch,value:True)
    def begin(self,*a):pass
    def close(self):pass
    def interrupt(self,*a):pass
    def outgoing_opus(self,*a):return []
class Tests(unittest.TestCase):
    def setUp(self):
        self.now=10.;self.events=[]
        self.s=StreamingSession(Registry(),ConversationSessions(),Bridge(),'synthetic-fixture','synthetic-device',lambda epoch:None,clock=lambda:self.now,fixture=True)
        self.s.begin('audio-fixture');self.s.control({'type':'ack','epoch':1});self.s.control({'type':'consent','epoch':1,'physical':True})
        self.d=PairedSessionCameraPermissionDispatcher(self.s,CameraStartCustodian(),clock=lambda:self.now,nonce=lambda:'a'*32)
        self.s.camera_dispatcher=self.d
    def prepare(self,ttl=10):
        return self.d.handle_local_request(dict(type='camera_permission_prepare',version=1,request_id='camera-fixture',mode='webcam',ttl_seconds=ttl))
    def reply(self,p,confirmed=False):
        v=dict(p,type='camera_permission_confirmation',state='confirmed' if confirmed else 'unavailable')
        if confirmed:v.update(generation='1',epoch='1',authority_id='7',boot_id='b'*32,expires_ms='20000')
        else:v['reason']='camera_driver_unqualified'
        return v
    def test_audio_authority_alone_never_camera_permission(self):
        self.assertTrue(self.s.armed);self.assertFalse(self.d.readiness()['permission_current'])
        self.d.handle_control(dict(type='camera_permission_confirmation',state='confirmed'))
        self.assertIsNone(self.d.authority)
    def test_real_worker_local_pipe_creates_only_proposal(self):
        def no_runtime():raise AssertionError('credential/runtime loader used')
        w=RememberedWorker(no_runtime,emit=self.events.append);w.gateway=NS(_activity_session=self.s)
        self.assertTrue(w.local_camera_command(dict(type='camera_permission_prepare',version=1,request_id='camera-fixture',mode='webcam',ttl_seconds=10)))
        p=self.d.pop_control();self.assertEqual(p['type'],'camera_permission_proposal');self.assertEqual(p['turn_id'],self.s.turn.id)
        self.assertFalse(self.events[-1]['permission_current']);self.assertIsNone(w.runtime)
        self.d.handle_control(self.reply(p));w.publish_activity()
        self.assertTrue(any('camera_driver_unqualified' in event.get('reason','') for event in self.events))
        self.assertIsNone(self.d.backend.retained_authority(self.s))
    def test_real_worker_without_admitted_session_denies_prepare(self):
        def no_runtime():raise AssertionError('credential/runtime loader used')
        w=RememberedWorker(no_runtime,emit=self.events.append)
        self.assertTrue(w.local_camera_command(dict(type='camera_permission_prepare',version=1,request_id='fixture',mode='webcam',ttl_seconds=10)))
        self.assertFalse(self.events[-1]['permission_current']);self.assertIsNone(w.runtime)
    def test_default_device_denial_never_issues(self):
        self.prepare();p=self.d.pop_control();self.d.handle_control(self.reply(p))
        self.assertIsNone(self.d.authority);self.assertIsNone(self.d.backend.retained_authority(self.s))
    def test_synthetic_paired_confirmation_exact_owner_capture_false(self):
        self.prepare();p=self.d.pop_control();self.d.handle_control(self.reply(p,True))
        a=self.d.authority;self.assertIsNotNone(a)
        commit=self.d.pop_control();self.assertEqual(commit['type'],'camera_permission_commit')
        self.assertEqual(commit['authority_id'],'7');self.assertEqual(commit['nonce'],p['nonce'])
        self.assertLessEqual(commit['remaining_seconds'],p['ttl_seconds'])
        self.assertEqual(a.owner.owner_purpose_id,p['nonce']);self.assertEqual(a.authority_id,7)
        self.assertFalse(self.d.readiness()['enabled']);self.assertFalse(self.d.readiness()['capture_ready'])
        self.assertIsNone(self.d.backend.capture_receipt(None))
        self.d.handle_local_request(dict(type='camera_permission_off',version=1,request_id='off'))
        self.assertIsNone(self.d.backend.current_authority(self.s,a));self.assertIs(self.d.backend.retained_authority(self.s),a)
        self.assertEqual(self.d.pop_control()['type'],'camera_permission_cancel')
    def test_off_pending_fences_late_reply(self):
        self.prepare();p=self.d.pop_control()
        self.d.handle_local_request(dict(type='camera_permission_off',version=1,request_id='off'))
        self.d.handle_control(self.reply(p,True));self.assertIsNone(self.d.authority)
    def test_schema_tuple_replay_and_replacement_turn_deny(self):
        for key,value in [('nonce','c'*32),('turn_generation','2'),('device_id','wrong-device'),('audio_epoch','2')]:
            with self.subTest(key=key):
                self.setUp();self.prepare();p=self.d.pop_control();r=self.reply(p,True);r[key]=value
                self.d.handle_control(r);self.assertIsNone(self.d.authority)
        self.setUp();self.prepare();p=self.d.pop_control();self.s.sessions.interrupt_turn(self.s.conversation.id,self.s.turn.id)
        self.d.handle_control(self.reply(p,True));self.assertIsNone(self.d.authority)
    def test_proposal_expiry_and_budget(self):
        self.prepare(1);p=self.d.pop_control();self.now=11.1;self.s.deadline=20
        self.d.tick();self.d.handle_control(self.reply(p,True));self.assertIsNone(self.d.authority)
        self.setUp();self.prepare();p=self.d.pop_control();self.d.handle_control(self.reply(p))
        self.prepare();self.assertIsNone(self.d._mac)
    def test_consumed_confirmation_replay_revokes_and_retains(self):
        self.prepare();p=self.d.pop_control();r=self.reply(p,True);self.d.handle_control(r)
        a=self.d.authority;self.assertIsNotNone(a)
        self.d.handle_control(r)
        self.assertIsNone(self.d.authority);self.assertIs(self.d.backend.retained_authority(self.s),a)
    def test_granted_expiry_emits_notice_and_retains_owner(self):
        self.prepare();p=self.d.pop_control();self.d.handle_control(self.reply(p,True));a=self.d.authority
        self.now=20.;self.s.deadline=30
        self.d.tick();self.assertIsNone(self.d.authority);self.assertIs(self.d.backend.retained_authority(self.s),a)
        notices=[]
        while (notice:=self.d.pop_notice()) is not None:notices.append(notice)
        self.assertTrue(any('expired' in notice['reason'] for notice in notices))
        self.assertEqual(self.d.pop_control()['type'],'camera_permission_cancel')
    def test_shared_gateway_custody_survives_replacement_session(self):
        self.prepare();p=self.d.pop_control();self.d.handle_control(self.reply(p,True))
        self.d.connection_lost();old=self.d.backend.retained_authority(self.s)
        self.assertTrue(self.d.custodian.pending(self.s.device_id))
        replacement=StreamingSession(Registry(),ConversationSessions(),Bridge(),'synthetic-fixture','synthetic-device',lambda epoch:None,clock=lambda:self.now,fixture=True)
        replacement.begin('new-audio-fixture');replacement.control({'type':'ack','epoch':1});replacement.control({'type':'consent','epoch':1,'physical':True})
        d=PairedSessionCameraPermissionDispatcher(replacement,self.d.custodian,clock=lambda:self.now,nonce=lambda:'c'*32)
        replacement.camera_dispatcher=d
        status=d.handle_local_request(dict(type='camera_permission_prepare',version=1,request_id='new-camera',mode='webcam',ttl_seconds=10))
        self.assertIs(d.backend,self.d.backend);self.assertFalse(status['permission_current'])
        self.assertIs(self.d.backend.retained_authority(self.s),old)
        self.assertIsNone(d.backend.retained_authority(replacement))
        self.assertFalse(d.backend.release_after_checked_stop(self.s,old,object()))
    def test_real_gateway_factory_attaches_wire_dispatcher(self):
        g=host_audio_device_gateway(Registry(),ConversationSessions(),HostAudioDeviceRoute('synthetic-input','synthetic-output'))
        self.assertIs(g.camera_dispatcher_factory,PairedSessionCameraPermissionDispatcher)
    def test_actual_connection_pump_dispatches_confirmation(self):
        self.prepare();p=self.d.pop_control();reply=self.reply(p)
        class Socket:
            def __init__(self):self.n=0;self.sent=[]
            async def recv(self):
                self.n+=1
                if self.n==1:return json.dumps(reply)
                raise RuntimeError('synthetic fixture complete')
            async def send(self,message):self.sent.append(message)
            async def close(self):pass
        socket=Socket()
        with self.assertRaises(RuntimeError):asyncio.run(run_connection(socket,self.s,duration=1))
        self.assertIsNone(self.d.authority);self.assertIsNone(self.d.backend.retained_authority(self.s))
if __name__=='__main__':unittest.main()
