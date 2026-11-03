"""Disabled Siri/task adapter seam: review an existing selected agent session.

No AppIntent registration, launch, agent request, context copy, fallback, session
creation, credential storage or execution is supplied. Fixture mode is explicit.
"""
from dataclasses import dataclass,field
from hashlib import sha256
import json
from home_lab.contracts import AgentConnection,Approval
from home_companion import ExistingSessionBinding,accepts_existing_session,_identifier

@dataclass(frozen=True)
class AgentHandoffDraft:
    request_id: str
    binding: ExistingSessionBinding
    source_id: str
    character_id: str
    task: str = field(repr=False)
    revision: str
    approval: Approval
    execution_available: bool = False

class ExistingAgentHandoff:
    def __init__(self,registry,sessions,*,connection_reader=None,fixture=False):
        if type(fixture) is not bool:raise ValueError('explicit fixture mode required')
        self.registry=registry;self.sessions=sessions;self._reader=connection_reader;self.fixture=fixture;self._drafts={}
    def draft(self,credential,binding,task,request_id):
        if type(task) is not str or not task.strip() or len(task)>4096 or not _identifier(request_id):
            raise ValueError('bounded task and request ID required')
        if not accepts_existing_session(self.registry,self.sessions,credential,binding,'agent_handoff'):
            raise PermissionError('session_unavailable')
        if not self.fixture or self._reader is None:raise PermissionError('adapter_unavailable')
        try:connection=self._reader(binding.connection_id)
        except Exception:raise PermissionError('adapter_unavailable') from None
        if type(connection) is not AgentConnection or connection.id!=binding.connection_id or connection.source_id not in ('codex','claude','hermes') or not _identifier(connection.character_id):
            raise PermissionError('adapter_unavailable')
        payload={'binding':(binding.device_id,binding.conversation_id,binding.turn_id,binding.generation,binding.connection_id,binding.external_session_id),
            'source':connection.source_id,'character':connection.character_id,'task':task}
        revision=sha256(json.dumps(payload,sort_keys=True,separators=(',',':')).encode()).hexdigest()
        previous=self._drafts.get(request_id)
        if previous is not None:
            if previous.revision!=revision:raise ValueError('request ID conflict')
            return previous
        if len(self._drafts)>=128:raise ValueError('draft review capacity reached')
        approval=Approval(id='stackchan-agent-review-'+revision,operation='agent.existing_session_task',target=binding.connection_id,
            revision=revision,conversation_id=binding.conversation_id,turn_id=binding.turn_id,status='pending')
        draft=AgentHandoffDraft(request_id,binding,connection.source_id,connection.character_id,task,revision,approval)
        self._drafts[request_id]=draft
        return draft
