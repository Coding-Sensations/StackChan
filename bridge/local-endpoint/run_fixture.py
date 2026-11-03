"""Finite synthetic client/server measurement. No hardware/model/audio devices."""
import asyncio,json,statistics,time,resource
from codec import OpusCodec
from endpoint import serve_fixture,OpusEnvelope
from test_codec import tone


async def main():
    from websockets.asyncio.client import connect
    values=[];outputs=0;server=await serve_fixture();port=server.sockets[0].getsockname()[1]
    begin=time.perf_counter()
    try:
        async with asyncio.timeout(15):
            async with connect(f'ws://127.0.0.1:{port}/synthetic',additional_headers={'X-StackChan-Fixture':'synthetic-only'},max_size=4112,max_queue=2,compression=None,proxy=None) as ws:
                await ws.send(json.dumps({'type':'hello','version':2,'transport':'websocket','audio_params':{'format':'opus','sample_rate':16000,'channels':1,'frame_duration':60}}))
                hello=json.loads(await ws.recv());assert hello['audio_params']['sample_rate']==24000
                with OpusCodec(16000) as source,OpusCodec(24000) as sink:
                    # First packet fills streaming filter/frame buffer; the second emits.
                    await ws.send(OpusEnvelope(2,source.encode(tone(16000,60))).encode())
                    for i in range(1,12):
                        start=time.perf_counter()
                        await ws.send(OpusEnvelope(2,source.encode(tone(16000,60)),i*60).encode())
                        packet=OpusEnvelope.decode(2,await asyncio.wait_for(ws.recv(),3))
                        assert len(sink.decode(packet.payload))==2880
                        outputs+=1;values.append((time.perf_counter()-start)*1000)
                    await ws.send(json.dumps({'type':'abort'}))
                    assert json.loads(await ws.recv())['queue_cleared']
                    await ws.send(json.dumps({'type':'fixture_finish'}))
                    assert json.loads(await ws.recv())['type']=='fixture_finished'
    finally:
        server.close();await server.wait_closed()
    print(json.dumps({'synthetic_only':True,'input_packets':12,'output_packets_before_abort':outputs,'streaming_first_output_requires_second60ms_packet':True,'loopback_encode_resample_decode_ms':{'min':min(values),'median':statistics.median(values),'max':max(values)},'model_latency':'unmeasured—no model called','wall_seconds':time.perf_counter()-begin,'max_rss_bytes':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,'listener_closed':not server.is_serving()},indent=2))
if __name__=='__main__':asyncio.run(main())
