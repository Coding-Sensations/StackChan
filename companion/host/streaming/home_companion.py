"""Apple/HOOBS status and review drafts only; import is inert.

Reuses existing HomeLab device/session/Approval contracts. No credential store,
HTTP client, discovery, action executor, notification or scheduler is supplied.
The default reader is unavailable; fixture readers are explicitly synthetic.
"""
from dataclasses import dataclass, field
from datetime import datetime
from hashlib import sha256
import json
from zoneinfo import ZoneInfo
from home_lab.contracts import Approval


def _identifier(value):
    return type(value) is str and 1<=len(value)<=128 and all(c.isascii() and (c.isalnum() or c in '-_.:') for c in value)

@dataclass(frozen=True)
class ExistingSessionBinding:
    device_id: str
    conversation_id: str
    turn_id: str
    generation: int
    connection_id: str
    external_session_id: str = field(repr=False)

def accepts_existing_session(registry,sessions,credential,binding,capability):
    """Validate an already selected session; never create or resume one."""
    if type(binding) is not ExistingSessionBinding or type(binding.generation) is not int or binding.generation<1:return False
    try:
        if registry.authenticate(credential)!=binding.device_id:return False
        device=registry.get(binding.device_id)
        if device is None or device.status!='active' or capability not in device.capabilities:return False
        conversation=sessions.get(binding.conversation_id);turn=sessions.get_turn(binding.turn_id)
        return (conversation.device_id==binding.device_id and conversation.status=='open'
            and conversation.connection_id==binding.connection_id
            and conversation.external_session_id==binding.external_session_id
            and type(binding.external_session_id) is str and 1<=len(binding.external_session_id)<=128
            and turn.conversation_id==conversation.id and turn.generation==binding.generation
            and sessions.accepts_output(conversation.id,turn.id,binding.generation))
    except (KeyError,ValueError,TypeError):return False

@dataclass(frozen=True)
class LightState:
    accessory_id: str
    brightness: int
    def __post_init__(self):
        if not _identifier(self.accessory_id) or type(self.brightness) is not int or not 0<=self.brightness<=100:
            raise ValueError('invalid light state')

@dataclass(frozen=True)
class RoomSnapshot:
    room_id: str
    lights: tuple[LightState,...]
    tv_active: bool = False
    def __post_init__(self):
        if not _identifier(self.room_id) or type(self.lights) is not tuple or len(self.lights)>32 or type(self.tv_active) is not bool:
            raise ValueError('invalid room snapshot')
        if any(type(light) is not LightState for light in self.lights) or len({light.accessory_id for light in self.lights})!=len(self.lights):
            raise ValueError('invalid room lights')

@dataclass(frozen=True)
class QuietHours:
    start_minute: int
    end_minute: int
    timezone: str
    def __post_init__(self):
        if any(type(v) is not int or not 0<=v<1440 for v in (self.start_minute,self.end_minute)) or self.start_minute==self.end_minute:
            raise ValueError('invalid quiet hours')
        ZoneInfo(self.timezone)
    def contains(self,when: datetime) -> bool:
        if not isinstance(when,datetime) or when.tzinfo is None:raise ValueError('aware time required')
        local=when.astimezone(ZoneInfo(self.timezone));minute=local.hour*60+local.minute
        if self.start_minute<self.end_minute:return self.start_minute<=minute<self.end_minute
        return minute>=self.start_minute or minute<self.end_minute

@dataclass(frozen=True)
class HomeStatus:
    state: str
    snapshot: RoomSnapshot | None = None

@dataclass(frozen=True)
class HomeReviewDraft:
    request_id: str
    binding: ExistingSessionBinding
    room_id: str
    operation: str
    targets: tuple[LightState,...]
    restore_targets: tuple[LightState,...]
    revision: str
    approval: Approval
    notify_user: bool
    execution_available: bool = False

class HomeCompanion:
    def __init__(self,registry,sessions,*,snapshot_reader=None,fixture=False,quiet_hours=None,clock=None):
        if type(fixture) is not bool:raise ValueError('explicit fixture mode required')
        self.registry=registry;self.sessions=sessions;self._reader=snapshot_reader;self.fixture=fixture
        self.quiet_hours=quiet_hours;self.clock=clock
        self._drafts={}
    def _authorized(self,credential,binding):
        return accepts_existing_session(self.registry,self.sessions,credential,binding,'home_status')
    def status(self,credential,binding,room_id):
        if not _identifier(room_id) or room_id.lower() in ('all','all_rooms','all-rooms'):raise ValueError('one selected room required')
        if not self._authorized(credential,binding):return HomeStatus('session_unavailable')
        if not self.fixture or self._reader is None:return HomeStatus('adapter_unavailable')
        try:snapshot=self._reader(room_id)
        except Exception:return HomeStatus('status_unavailable')
        if type(snapshot) is not RoomSnapshot or snapshot.room_id!=room_id:return HomeStatus('status_unavailable')
        return HomeStatus('ready',snapshot)
    def _review(self,binding,room_id,operation,request_id,targets,restore_targets):
        if not _identifier(request_id):raise ValueError('bounded request ID required')
        payload={'binding':(binding.device_id,binding.conversation_id,binding.turn_id,binding.generation,binding.connection_id,binding.external_session_id),
            'room':room_id,'operation':operation,'targets':[(v.accessory_id,v.brightness) for v in targets],
            'restore_targets':[(v.accessory_id,v.brightness) for v in restore_targets]}
        revision=sha256(json.dumps(payload,sort_keys=True,separators=(',',':')).encode()).hexdigest()
        previous=self._drafts.get(request_id)
        if previous is not None:
            if previous.revision!=revision:raise ValueError('request ID conflict')
            return previous
        if len(self._drafts)>=128:raise ValueError('draft review capacity reached')
        if self.quiet_hours is not None:
            if self.clock is None:raise ValueError('explicit quiet-hours clock required')
            notify=not self.quiet_hours.contains(self.clock())
        else:notify=False # Alerts remain off until a room/time policy is explicitly supplied.
        approval=Approval(id='stackchan-review-'+revision,operation='home.'+operation,target=room_id,
            revision=revision,conversation_id=binding.conversation_id,turn_id=binding.turn_id,status='pending')
        draft=HomeReviewDraft(request_id,binding,room_id,operation,targets,restore_targets,revision,approval,notify)
        self._drafts[request_id]=draft
        return draft
    def draft(self,credential,binding,room_id,operation,request_id,*,brightness):
        if operation not in ('dim','brighten') or type(brightness) is not int or not 0<=brightness<=100:
            raise ValueError('bounded light draft required')
        result=self.status(credential,binding,room_id)
        if result.state!='ready':raise PermissionError(result.state)
        snapshot=result.snapshot
        if not snapshot.lights:raise ValueError('no selected lights')
        targets=tuple(LightState(v.accessory_id,brightness) for v in snapshot.lights)
        return self._review(binding,room_id,operation,request_id,targets,snapshot.lights)
    def draft_restore(self,credential,binding,previous,request_id):
        if type(previous) is not HomeReviewDraft or previous.binding!=binding or self._drafts.get(previous.request_id)!=previous:
            raise ValueError('known session-bound draft required')
        result=self.status(credential,binding,previous.room_id)
        if result.state!='ready':raise PermissionError(result.state)
        if {v.accessory_id for v in result.snapshot.lights}!={v.accessory_id for v in previous.restore_targets}:
            raise ValueError('selected accessories changed')
        return self._review(binding,previous.room_id,'restore',request_id,previous.restore_targets,result.snapshot.lights)
