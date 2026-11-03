import asyncio
import unittest
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
from remote_gateway import RemoteGateway,QualifiedProvider
from provisioning import ProvisioningBroker,unpack_grant
from streaming_session import encode_frame
from test_streaming import Bridge
from websockets.asyncio.client import connect
from websockets.exceptions import InvalidStatus

class Tests(unittest.IsolatedAsyncioTestCase):
    async def test_full_offline_provisioning_websocket_and_revoke(self):
        registry=DeviceRegistry();broker=ProvisioningBroker(registry,'wss://synthetic.example/companion/audio')
        grant=broker.redeem(registry.create_pairing_code(),device_id='synthetic-provisioning',label='Synthetic fixture',request_id='fixture-request',owner_authorized=True,route_qualified=True)
        _,device,credential,request=unpack_grant(grant.frame)
        bridges=[];closed=[];arms=[]
        def provider(_):
            b=Bridge();bridges.append(b)
            return QualifiedProvider(b,arms.append,lambda:closed.append(True),'synthetic-session',True,'synthetic_fixture')
        runtime=RemoteGateway(registry,ConversationSessions(),provider,fixture=True)
        self.assertEqual(runtime.activity_state,'physical_consent')
        async with asyncio.timeout(5):
            async with runtime.listener(port=0) as server:
                uri=f'ws://127.0.0.1:{server.sockets[0].getsockname()[1]}/companion/audio'
                with self.assertRaises(InvalidStatus) as denied:
                    async with connect(uri,proxy=None):pass
                self.assertEqual(denied.exception.response.status_code,403)
                headers={'Authorization':'Bearer '+credential,'X-Companion-Device':device}
                async with connect(uri,proxy=None,additional_headers=headers,max_size=4112,compression=None) as ws:
                    import json
                    await ws.send(json.dumps({'type':'begin','request_id':request}));hello=json.loads(await ws.recv())
                    self.assertTrue(hello['fixture']) # deliberately unusable on real firmware
                    await ws.send(json.dumps({'type':'ack','epoch':1}));self.assertEqual(json.loads(await ws.recv())['type'],'acknowledged')
                    await ws.send(json.dumps({'type':'consent','epoch':1,'physical':True}));self.assertEqual(json.loads(await ws.recv())['type'],'armed')
                    await ws.send(encode_frame(1,0,False,b'synthetic-not-opus'))
                    for _ in range(10):
                        if bridges[0].input:break
                        await asyncio.sleep(.01)
                    self.assertEqual(len(bridges[0].input),1);self.assertEqual(arms,[1]);self.assertEqual(runtime.activity_state,'conversation')
                    registry.revoke(device)
                    await asyncio.wait_for(ws.wait_closed(),.5)
            self.assertFalse(runtime._active);self.assertEqual(closed,[True]);self.assertEqual(bridges[0].input,[]);self.assertEqual(runtime.activity_state,'physical_consent')
    async def test_production_no_adapter_returns_503(self):
        registry=DeviceRegistry();device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='synthetic',label='Synthetic',capabilities=('microphone','speaker'))
        # Fixture socket only; no production TLS listener is opened.
        runtime=RemoteGateway(registry,ConversationSessions(),fixture=True)
        async with asyncio.timeout(3):
            async with runtime.listener(port=0) as server:
                uri=f'ws://127.0.0.1:{server.sockets[0].getsockname()[1]}/companion/audio'
                with self.assertRaises(InvalidStatus) as denied:
                    async with connect(uri,proxy=None,additional_headers={'Authorization':'Bearer '+token,'X-Companion-Device':device.id}):pass
                self.assertEqual(denied.exception.response.status_code,503)
    async def test_rejects_insecure_production_and_external_fixture_binds(self):
        registry=DeviceRegistry();sessions=ConversationSessions()
        with self.assertRaises(ValueError):RemoteGateway(registry,sessions).listener(host='0.0.0.0')
        with self.assertRaises(ValueError):RemoteGateway(registry,sessions).listener(host='0.0.0.0',trusted_tls_proxy=True)
        with self.assertRaises(ValueError):RemoteGateway(registry,sessions,fixture=True).listener(host='0.0.0.0')
    async def test_separate_api_persona_is_not_existing_mira(self):
        from types import SimpleNamespace
        from websockets.datastructures import Headers
        registry=DeviceRegistry();device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='synthetic-provider-gate',label='Synthetic',capabilities=('microphone','speaker'))
        closed=[];socket_closes=[]
        async def close(**kw):socket_closes.append(kw)
        socket=SimpleNamespace(request=SimpleNamespace(path='/companion/audio',headers=Headers([('Authorization','Bearer '+token),('X-Companion-Device',device.id)])),close=close)
        provider=lambda _:QualifiedProvider(Bridge(),lambda _:None,lambda:closed.append(True),'synthetic-api-reference',True,'api_persona')
        runtime=RemoteGateway(registry,ConversationSessions(),provider)
        await runtime.handle(socket)
        self.assertEqual(socket_closes[0]['code'],1008);self.assertEqual(closed,[True]);self.assertFalse(runtime._active)
    async def test_unverified_identity_denied_before_arm(self):
        from types import SimpleNamespace
        from websockets.datastructures import Headers
        registry=DeviceRegistry();device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='synthetic-unverified',label='Synthetic',capabilities=('microphone','speaker'))
        arms=[];closes=[]
        async def close(**kw):closes.append(kw)
        socket=SimpleNamespace(request=SimpleNamespace(path='/companion/audio',headers=Headers([('Authorization','Bearer '+token),('X-Companion-Device',device.id)])),close=close)
        runtime=RemoteGateway(registry,ConversationSessions(),lambda _:QualifiedProvider(Bridge(),arms.append,lambda:None,'synthetic-reference',False,'existing_mira'))
        await runtime.handle(socket)
        self.assertEqual(arms,[]);self.assertEqual(closes[0]['code'],1008);self.assertFalse(runtime._active)
    async def test_busy_device_does_not_open_second_provider(self):
        from types import SimpleNamespace
        from websockets.datastructures import Headers
        registry=DeviceRegistry();device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='synthetic-busy',label='Synthetic',capabilities=('microphone','speaker'))
        factories=[];closes=[]
        async def close(**kw):closes.append(kw)
        socket=SimpleNamespace(request=SimpleNamespace(path='/companion/audio',headers=Headers([('Authorization','Bearer '+token),('X-Companion-Device',device.id)])),close=close)
        runtime=RemoteGateway(registry,ConversationSessions(),lambda _:factories.append(True))
        runtime._active.add(device.id);await runtime.handle(socket)
        self.assertEqual(factories,[]);self.assertEqual(closes[0]['code'],1008);self.assertIn(device.id,runtime._active)
if __name__=='__main__':unittest.main()
