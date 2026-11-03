"""Existing-session task review fixtures; no Siri/runtime launch or new session."""
import unittest
from home_lab.contracts import AgentConnection
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
from home_companion import ExistingSessionBinding
import agent_handoff as handoff

class AgentHandoffTests(unittest.TestCase):
    def setUp(self):
        self.registry=DeviceRegistry()
        self.device,self.token=self.registry.redeem_pairing_code(self.registry.create_pairing_code(),device_id='fixture-robot',label='Fixture',capabilities=('agent_handoff',))
        self.sessions=ConversationSessions();self.connections={};self.bindings={}
        for source in ('codex','claude','hermes'):
            connection=source+'.fixture'
            self.connections[connection]=AgentConnection(id=connection,source_id=source,character_id=source,runtime=source,profile='fixture',model='fixture',host='fixture',billing='fixture')
            conversation=self.sessions.create(connection_id=connection,device_id=self.device.id)
            self.sessions.attach_external_session(conversation.id,'existing-'+source)
            turn=self.sessions.begin_turn(conversation.id,request_id='fixture-'+source)
            self.bindings[source]=ExistingSessionBinding(self.device.id,conversation.id,turn.id,turn.generation,connection,'existing-'+source)
        self.seam=handoff.ExistingAgentHandoff(self.registry,self.sessions,connection_reader=self.connections.get,fixture=True)
    def test_default_adapter_never_looks_up_or_launches(self):
        seam=handoff.ExistingAgentHandoff(self.registry,self.sessions,connection_reader=lambda _:self.fail('disabled reader invoked'))
        with self.assertRaisesRegex(PermissionError,'adapter_unavailable'):
            seam.draft(self.token,self.bindings['codex'],'Review the task','one')
    def test_each_agent_preserves_existing_context_and_identity(self):
        before=self.sessions.all()
        for source,binding in self.bindings.items():
            draft=self.seam.draft(self.token,binding,'Review this task',source+'-request')
            self.assertEqual(draft.binding,binding);self.assertEqual(draft.character_id,source)
            self.assertEqual(draft.source_id,source);self.assertEqual(draft.approval.status,'pending')
            self.assertEqual(draft.approval.turn_id,binding.turn_id)
            self.assertFalse(draft.execution_available);self.assertFalse(hasattr(self.seam,'execute'))
            self.assertNotIn('Review this task',repr(draft))
        self.assertEqual(self.sessions.all(),before)
    def test_no_fallback_stale_or_revoked_session(self):
        binding=self.bindings['codex'];self.connections.pop(binding.connection_id)
        with self.assertRaisesRegex(PermissionError,'adapter_unavailable'):self.seam.draft(self.token,binding,'Review','missing')
        self.assertEqual(len(self.connections),2)
        self.sessions.interrupt_turn(binding.conversation_id,binding.turn_id)
        with self.assertRaisesRegex(PermissionError,'session_unavailable'):self.seam.draft(self.token,binding,'Review','stale')
        self.registry.revoke(self.device.id)
        with self.assertRaisesRegex(PermissionError,'session_unavailable'):self.seam.draft(self.token,self.bindings['hermes'],'Review','revoked')
    def test_dedupe_conflict_bounds_and_foreign_device(self):
        binding=self.bindings['claude'];draft=self.seam.draft(self.token,binding,'Review','request')
        self.assertIs(self.seam.draft(self.token,binding,'Review','request'),draft)
        with self.assertRaises(ValueError):self.seam.draft(self.token,binding,'Different task','request')
        for task in ('',' ', 'x'*4097):
            with self.assertRaises(ValueError):self.seam.draft(self.token,binding,task,'bad')
        foreign=ExistingSessionBinding('other',binding.conversation_id,binding.turn_id,binding.generation,binding.connection_id,binding.external_session_id)
        with self.assertRaisesRegex(PermissionError,'session_unavailable'):self.seam.draft(self.token,foreign,'Review','foreign')
