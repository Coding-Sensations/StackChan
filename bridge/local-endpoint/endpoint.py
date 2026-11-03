"""Finite localhost synthetic factory-protocol fixture; never a live robot server."""
import asyncio
import json
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'transport'))
# Dedicated layout is bridge/local-endpoint, sibling transport.
from transport import OpusEnvelope
from codec import OpusCodec,Resampler


def parse_hello(value):
    if not isinstance(value,dict) or value.get('type')!='hello' or type(value.get('version')) is not int or value['version'] not in (1,2,3) or value.get('transport')!='websocket':raise ValueError('hello')
    if value.get('audio_params')!={'format':'opus','sample_rate':16000,'channels':1,'frame_duration':60}:raise ValueError('expected factory16k mono60ms Opus')
    return value['version']


class Pipeline:
    """Real Opus16k -> PCM48k host bus -> PCM24k -> Opus24k, bounded."""
    def __init__(self):
        self.input=OpusCodec(16000);self.output=OpusCodec(24000)
        self.up=Resampler(16000,48000);self.down=Resampler(48000,24000)
        self.pending=bytearray()
    def _pack(self,pcm):
        self.pending.extend(pcm)
        if len(self.pending)>8640:raise ValueError('output queue bound')
        result=[]
        while len(self.pending)>=2880:
            result.append(self.output.encode(bytes(self.pending[:2880])));del self.pending[:2880]
        return result
    def feed(self,packet):
        pcm=self.input.decode(packet)
        if len(pcm)!=1920:raise ValueError('required60ms audio packet')
        bus=self.up.process(pcm)
        return self._pack(self.down.process(bus) if bus else b'')
    def finish(self):
        bus=self.up.flush();pcm=self.down.process(bus) if bus else b''
        return self._pack(pcm+self.down.flush())
    def reset(self):
        self.pending.clear()
        for obj in (self.input,self.output,self.up,self.down):obj.reset()
    def close(self):
        self.pending.clear()
        for obj in (self.input,self.output,self.up,self.down):obj.close()
    def __enter__(self):return self
    def __exit__(self,*args):self.close()


async def fixture_handler(ws):
    """At most 32 packets, 5sec idle, 20sec connection; synthetic opt-in only."""
    try:
        async with asyncio.timeout(20):
            first=await asyncio.wait_for(ws.recv(),5)
            if not isinstance(first,str):raise ValueError('hello must be JSON')
            version=parse_hello(json.loads(first))
            await ws.send(json.dumps({'type':'hello','transport':'websocket','session_id':'synthetic-only','audio_params':{'format':'opus','sample_rate':24000,'channels':1,'frame_duration':60}}))
            with Pipeline() as pipeline:
                count=0
                while True:
                    message=await asyncio.wait_for(ws.recv(),5)
                    if isinstance(message,bytes):
                        count+=1
                        if count>32:raise ValueError('packet limit')
                        packets=pipeline.feed(OpusEnvelope.decode(version,message).payload)
                        for packet in packets:await ws.send(OpusEnvelope(version,packet).encode())
                    else:
                        command=json.loads(message)
                        if command=={'type':'abort'}:
                            pipeline.reset();await ws.send(json.dumps({'type':'fixture_reset','queue_cleared':True}))
                        elif command=={'type':'fixture_finish'}:
                            for packet in pipeline.finish():await ws.send(OpusEnvelope(version,packet).encode())
                            await ws.send(json.dumps({'type':'fixture_finished'}));break
                        else:raise ValueError('fixture command')
    except (ValueError,TimeoutError,json.JSONDecodeError):
        await ws.close(code=1008,reason='synthetic fixture policy')


class Admission:
    def __init__(self):self.accepted=False
    def check(self,connection,request):
        headers=request.headers
        if self.accepted or request.path!='/synthetic' or headers.get('X-StackChan-Fixture')!='synthetic-only' or any(name in headers for name in ('Authorization','Device-Id','Client-Id','Origin')):
            return connection.respond(403,'Only one credential-free synthetic fixture is permitted.\n')
        self.accepted=True
        return None


async def serve_fixture():
    from websockets.asyncio.server import serve
    admission=Admission()
    return await serve(fixture_handler,'127.0.0.1',0,process_request=admission.check,max_size=4112,max_queue=2,compression=None,open_timeout=3,close_timeout=1,ping_interval=None)
