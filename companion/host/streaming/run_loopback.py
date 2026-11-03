"""Finite localhost authenticated synthetic network fixture; no real credentials/IO.
Actual HomeLab in-memory pairing, WebSocket handshake and native Opus worker.
The only keys created here authorize one synthetic device inside this process.
"""
import asyncio,json
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
from host_adapter import SyntheticHostAdapter,RoutePlan,CodecHostBridge
from codec import OpusCodec,Resampler
from streaming_session import StreamingSession,parse_control,encode_frame,decode_frame
from websockets.asyncio.server import serve
from websockets.asyncio.client import connect
async def main():
    registry=DeviceRegistry();code=registry.create_pairing_code()
    device,token=registry.redeem_pairing_code(code,device_id='synthetic-stackchan',label='Synthetic only',capabilities=('microphone','speaker'))
    sessions=ConversationSessions();adapter=SyntheticHostAdapter(RoutePlan('fixture-A','fixture-B'))
    components=[OpusCodec(16000),OpusCodec(24000),Resampler(16000,48000),Resampler(48000,24000)]
    bridge=CodecHostBridge(adapter,*components);stats={'received':0,'returned':0,'interrupt':False,'unauthorized_rejected':False};accepted=False
    def admission(connection,request):
        nonlocal accepted
        try:
            auth=request.headers.get('Authorization','');identity=request.headers.get('X-Companion-Device','')
            if accepted or request.path!='/synthetic' or request.headers.get('X-StackChan-Fixture')!='synthetic-only' or not auth.startswith('Bearer ') or registry.authenticate(auth[7:])!=identity or identity!=device.id:raise PermissionError()
        except Exception:return connection.respond(403,'Synthetic fixture admission denied.\n')
        accepted=True;return None
    async def handler(ws):
        session=StreamingSession(registry,sessions,bridge,token,device.id,adapter.unmute_synthetic,fixture=True)
        try:
            async with asyncio.timeout(10):
                for _ in range(20):
                    message=await asyncio.wait_for(ws.recv(),2)
                    if isinstance(message,bytes):
                        session.receive(message);stats['received']+=1
                        # Generate a return stream separately; never loop captured mic into itself.
                        adapter.dot_output_callback(session.epoch,b'\0'*5760)
                        output=[]
                        for _ in range(6):output.extend(session.outgoing(480))
                        for wire in output:await ws.send(wire);stats['returned']+=1
                        await ws.send(json.dumps({'type':'fixture_audio_done'}))
                    else:
                        value=parse_control(message);answer=session.control(value);await ws.send(json.dumps(answer))
                        if value['type']=='interrupt':stats['interrupt']=True;break
        finally:session.close()
    try:
        async with asyncio.timeout(15):
            async with serve(handler,'127.0.0.1',0,process_request=admission,max_size=4112,max_queue=2,compression=None,ping_interval=None,close_timeout=1) as server:
                port=server.sockets[0].getsockname()[1];url=f'ws://127.0.0.1:{port}/synthetic'
                try:
                    async with connect(url,proxy=None,additional_headers={'Authorization':'Bearer invalid-synthetic','X-Companion-Device':device.id,'X-StackChan-Fixture':'synthetic-only'},open_timeout=2):raise AssertionError('unauthorized accepted')
                except Exception as error:
                    if getattr(getattr(error,'response',None),'status_code',None)!=403:raise
                    stats['unauthorized_rejected']=True
                async with connect(url,proxy=None,additional_headers={'Authorization':'Bearer '+token,'X-Companion-Device':device.id,'X-StackChan-Fixture':'synthetic-only'},max_size=4112,max_queue=2,compression=None,ping_interval=None,open_timeout=2,close_timeout=1) as ws:
                    async def control(value):await ws.send(json.dumps(value));return json.loads(await ws.recv())
                    hello=await control({'type':'begin','request_id':'synthetic-loopback'});e=hello['epoch']
                    await control({'type':'ack','epoch':e});await control({'type':'consent','epoch':e,'physical':True})
                    with OpusCodec(16000) as encoder,OpusCodec(24000) as decoder:
                        for sequence in range(4):
                            await ws.send(encode_frame(e,sequence,False,encoder.encode(b'\0'*1920)))
                            while True:
                                answer=await asyncio.wait_for(ws.recv(),2)
                                if isinstance(answer,str):assert json.loads(answer)['type']=='fixture_audio_done';break
                                epoch,seq,speaker,packet=decode_frame(answer);assert epoch==e and speaker;assert len(decoder.decode(packet))==2880
                    answer=await control({'type':'interrupt','epoch':e});assert not answer['speaker_stop_confirmed'] and not answer['provider_cancel_confirmed']
        assert stats['received']==4 and stats['returned']>=3 and stats['interrupt'] and stats['unauthorized_rejected']
        assert adapter.muted and adapter.buffered_samples=={'robot_to_dot':0,'dot_to_robot':0}
        print(json.dumps({'result':'PASS',**stats,'real_device_io':False,'credential_scope':'in-memory synthetic device only','listener_closed':True}))
    finally:
        for component in components:component.close()
if __name__=='__main__':asyncio.run(main())
