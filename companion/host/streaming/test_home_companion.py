"""Synthetic status/draft integration. No home endpoint or automation exists."""
import unittest
from datetime import datetime, timezone
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
try:
    import home_companion as home
except ImportError:
    home=None

class HomeCompanionTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(home,'offline home companion seam is missing')
        self.registry=DeviceRegistry();self.device,self.token=self.registry.redeem_pairing_code(
            self.registry.create_pairing_code(),device_id='synthetic-robot',label='Fixture',capabilities=('home_status',))
        self.sessions=ConversationSessions();conversation=self.sessions.create(connection_id='mira.native_voice.transport',device_id=self.device.id)
        self.conversation=self.sessions.attach_external_session(conversation.id,'fixture-existing-Mira')
        self.turn=self.sessions.begin_turn(conversation.id,request_id='voice-turn')
        self.binding=home.ExistingSessionBinding(self.device.id,conversation.id,self.turn.id,self.turn.generation,'mira.native_voice.transport','fixture-existing-Mira')
        self.room=home.RoomSnapshot('living-room',(home.LightState('fixture-lifx',60),),tv_active=True)
        self.reads=[]
        def snapshot(room):self.reads.append(room);return self.room
        self.adapter=home.HomeCompanion(self.registry,self.sessions,snapshot_reader=snapshot,fixture=True)
    def test_default_is_unavailable_and_never_calls_reader(self):
        blocked=home.HomeCompanion(self.registry,self.sessions,snapshot_reader=lambda _:self.fail('live reader disabled'))
        self.assertEqual(blocked.status(self.token,self.binding,'living-room').state,'adapter_unavailable')
    def test_status_reuses_selected_session_and_selected_room(self):
        before=self.sessions.all()
        status=self.adapter.status(self.token,self.binding,'living-room')
        self.assertEqual(status.state,'ready');self.assertEqual(status.snapshot,self.room)
        self.assertEqual(self.reads,['living-room']);self.assertEqual(self.sessions.all(),before)
    def test_foreign_session_revocation_and_stale_turn_deny_before_read(self):
        foreign=home.ExistingSessionBinding('other-device',self.binding.conversation_id,self.binding.turn_id,self.binding.generation,self.binding.connection_id,self.binding.external_session_id)
        self.assertEqual(self.adapter.status(self.token,foreign,'living-room').state,'session_unavailable')
        self.sessions.interrupt_turn(self.binding.conversation_id,self.binding.turn_id)
        self.assertEqual(self.adapter.status(self.token,self.binding,'living-room').state,'session_unavailable')
        self.registry.revoke(self.device.id)
        self.assertEqual(self.adapter.status(self.token,self.binding,'living-room').state,'session_unavailable')
        self.assertEqual(self.reads,[])
    def test_quiet_hours_cross_midnight_and_room_scope(self):
        quiet=home.QuietHours(22*60,8*60,'Etc/UTC')
        for hour in (23,0,7):self.assertTrue(quiet.contains(datetime(2026,10,10,hour,tzinfo=timezone.utc)))
        self.assertFalse(quiet.contains(datetime(2026,10,10,8,tzinfo=timezone.utc)))
        self.assertFalse(quiet.contains(datetime(2026,10,10,21,tzinfo=timezone.utc)))
        with self.assertRaises(ValueError):home.QuietHours(0,0,'Etc/UTC')
        for room in ('','all','all_rooms','*'):
            with self.assertRaises(ValueError):self.adapter.status(self.token,self.binding,room)
    def test_dim_and_restore_are_immutable_review_drafts_only(self):
        plan=self.adapter.draft(self.token,self.binding,'living-room','dim','request-one',brightness=15)
        self.assertEqual(plan.targets,(home.LightState('fixture-lifx',15),))
        self.assertEqual(plan.restore_targets,self.room.lights)
        self.assertEqual(plan.approval.status,'pending');self.assertEqual(plan.approval.conversation_id,self.binding.conversation_id)
        self.assertEqual(plan.approval.turn_id,self.binding.turn_id);self.assertEqual(plan.approval.revision,plan.revision)
        self.assertFalse(plan.execution_available);self.assertFalse(hasattr(self.adapter,'execute'))
        self.assertEqual(self.adapter.draft(self.token,self.binding,'living-room','dim','request-one',brightness=15),plan)
        with self.assertRaises(ValueError):self.adapter.draft(self.token,self.binding,'living-room','dim','request-one',brightness=25)
        restore=self.adapter.draft_restore(self.token,self.binding,plan,'restore-one')
        self.assertEqual(restore.targets,self.room.lights)
    def test_reader_error_uses_fixed_label_and_no_private_text(self):
        def bad(_):raise RuntimeError('synthetic private endpoint')
        adapter=home.HomeCompanion(self.registry,self.sessions,snapshot_reader=bad,fixture=True)
        status=adapter.status(self.token,self.binding,'living-room')
        self.assertEqual(status.state,'status_unavailable');self.assertNotIn('private',repr(status))

    def test_quiet_room_policy_is_bound_to_draft_and_does_not_notify(self):
        quiet=home.QuietHours(22*60,8*60,'Etc/UTC')
        adapter=home.HomeCompanion(self.registry,self.sessions,snapshot_reader=lambda _:self.room,fixture=True,quiet_hours=quiet,clock=lambda:datetime(2026,10,10,23,tzinfo=timezone.utc))
        plan=adapter.draft(self.token,self.binding,'living-room','brighten','quiet-plan',brightness=70)
        self.assertFalse(plan.notify_user)
        self.assertFalse(plan.execution_available)
        self.assertEqual(plan.room_id,'living-room')
    def test_wrong_room_or_changed_accessories_deny_restore(self):
        adapter=home.HomeCompanion(self.registry,self.sessions,snapshot_reader=lambda _:home.RoomSnapshot('bedroom',()),fixture=True)
        self.assertEqual(adapter.status(self.token,self.binding,'living-room').state,'status_unavailable')
        plan=self.adapter.draft(self.token,self.binding,'living-room','dim','before',brightness=10)
        self.room=home.RoomSnapshot('living-room',(home.LightState('other-light',20),))
        with self.assertRaises(ValueError):self.adapter.draft_restore(self.token,self.binding,plan,'after')
