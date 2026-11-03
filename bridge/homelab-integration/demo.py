"""In-memory integration exercise, using bytes generated here, not a microphone."""
import json
from home_lab.contracts import Device
from home_lab.events import EventJournal
from home_lab.sessions import ConversationSessions
from adapter import AudioFrame, OfflineAdapter


def main():
    device = Device(id='stackchan-fixture',label='Offline StackChan fixture',
                    capabilities=('speaker','camera','motor'),status='offline_fixture')
    events = EventJournal()
    bridge = OfflineAdapter(device, ConversationSessions(), events)
    turn = bridge.begin('demo-request', 1.0)
    frame = AudioFrame(device_id=device.id,conversation_id=turn.conversation_id,
                      turn_id=turn.id,generation=turn.generation,sequence=0,pcm=b'\0\0'*320)
    assert bridge.push(AudioFrame.from_wire(frame.to_wire()),1.02)
    plan = bridge.plan_tool('camera.snapshot',{'width':320,'height':240,'max_bytes':262144})
    interruption = bridge.interrupt(1.03)
    assert bridge.pop(1.04) is None and not bridge.push(frame,1.05)
    _, entries = events.after_cursor(0)
    print(json.dumps({'device':device.to_dict(),'connection':'same-dot.unsupported',
        'frame_format':'fixture PCM16LE mono16kHz; not factory protocol',
        'camera_plan':{**plan,'approval':plan['approval'].to_dict()},
        'interrupt':interruption,'stale_frame_rejected':True,
        'events':[e.to_dict() for e in entries], 'live_transport':False},indent=2))


if __name__ == '__main__':
    main()
