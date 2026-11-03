"""Real reducer/authentication/callback custody; fixture keys and no live IO."""
import copy
import hashlib
import unittest
try:
    from companion_events import CompanionSession, RequestIdentity, sign_packet, project_hook, ProviderSelection
except ImportError:
    CompanionSession = None

KEY=b'event-fixture-key-not-a-credential!'
DECISION_KEY=b'decision-fixture-key-not-secret!!'
class Events(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(CompanionSession, 'provider event/approval custody is not implemented')
        self.now=10.;self.seq=0;self.called=[]
        self.s=CompanionSession('codex','binding1','generation1',KEY,DECISION_KEY,
                                scopes={'activity','approval'},clock=lambda:self.now)
    def packet(self,kind='working',task='task1',**fields):
        self.seq+=1
        packet=dict(version=1,provider='codex',binding='binding1',generation='generation1',sequence=self.seq,kind=kind,task=task)
        packet.update(fields);return packet
    def send(self,packet,key=KEY):return self.s.receive(packet,sign_packet(key,packet))
    def working(self,task='task1'):self.assertTrue(self.send(self.packet(task=task)))
    def request(self,rid='request1',digest='a'*64,task='task1',callback=None):
        identity=RequestIdentity('codex','binding1','generation1',task,rid,digest)
        self.s.register_request(identity,deadline=20.,respond=callback or (lambda choice:self.called.append(choice)))
        return identity
    def decision(self,identity,choice='approve',**overrides):
        p=self.packet('decision',identity.task,request_id=identity.request_id,
                      operation_sha256=identity.operation_sha256,choice=choice)
        p.update(overrides);return self.send(p,DECISION_KEY)
    def test_authenticated_work_and_parallel_completion(self):
        self.assertEqual(self.s.scene(),'unknown')
        self.working();self.working('task2');self.assertEqual(self.s.scene(),'desk_work')
        self.assertTrue(self.send(self.packet('completed')));self.assertEqual(self.s.scene(),'desk_work')
        self.assertTrue(self.send(self.packet('completed','task2')));self.assertEqual(self.s.scene(),'completion')
        self.assertEqual(self.s.take_completion(),('task1','task2'));self.assertEqual(self.s.take_completion(),())
        self.assertEqual(self.s.scene(),'idle_decoration')
    def test_foreign_provider_binding_generation_or_signature_never_changes_state(self):
        for name,value in [('provider','muse'),('binding','other'),('generation','retired')]:
            p=self.packet();p[name]=value;self.assertFalse(self.send(p))
        p=self.packet();self.assertFalse(self.s.receive(p,'0'*64));self.assertEqual(self.s.scene(),'unknown')
    def test_replay_and_terminal_revival_are_rejected(self):
        p=self.packet();self.assertTrue(self.send(p));self.assertFalse(self.send(p))
        self.assertTrue(self.send(self.packet('completed')))
        self.assertFalse(self.send(self.packet('working')));self.assertEqual(self.s.take_completion(),('task1',))
    def test_error_priority_and_disconnect_invalidates(self):
        self.working();self.working('task2');self.assertTrue(self.send(self.packet('error')))
        self.assertEqual(self.s.scene(),'error');self.s.disconnect();self.assertEqual(self.s.scene(),'unknown')
        self.assertFalse(self.send(self.packet('working','task3')))
    def test_approval_requires_local_exact_owner_and_matching_digest(self):
        self.working();r=self.request();self.assertEqual(self.s.scene(),'decision_sign')
        self.assertFalse(self.decision(r,operation_sha256='b'*64));self.assertFalse(self.decision(r,request_id='other'))
        p=self.packet('decision',request_id=r.request_id,operation_sha256=r.operation_sha256,choice='approve')
        self.assertFalse(self.send(p,KEY)) # activity authority cannot decide
        self.assertTrue(self.decision(r));self.assertEqual(self.called,['approve'])
        self.assertFalse(self.decision(r));self.assertEqual(self.s.request_state(r),'submitted')
        self.s.acknowledge(r);self.assertEqual(self.s.request_state(r),'acknowledged')
        self.assertEqual(self.s.scene(),'desk_work')
    def test_scope_separation_no_implicit_request(self):
        self.working();r=RequestIdentity('codex','binding1','generation1','task1','unknown','a'*64)
        self.assertFalse(self.decision(r));self.assertEqual(self.called,[])
        self.s=CompanionSession('codex','binding1','generation1',KEY,DECISION_KEY,scopes={'activity'},clock=lambda:self.now)
        self.working()
        with self.assertRaises(PermissionError):self.request()
    def test_expired_request_and_disconnect_do_not_answer(self):
        self.working();r=self.request();self.now=20.
        self.assertFalse(self.decision(r));self.assertEqual(self.called,[]);self.assertEqual(self.s.request_state(r),'expired')
        self.now=10.;r=self.request('request2');self.s.disconnect()
        self.assertFalse(self.decision(r));self.assertEqual(self.called,[])
    def test_uncertain_callback_is_never_retried(self):
        self.working()
        def uncertain(choice):self.called.append(choice);raise OSError('uncertain send')
        r=self.request(callback=uncertain);self.assertFalse(self.decision(r));self.assertEqual(self.s.request_state(r),'uncertain')
        self.assertFalse(self.decision(r));self.assertEqual(self.called,['approve'])
    def test_bad_request_identity_and_deadline_are_rejected(self):
        self.working()
        with self.assertRaises(ValueError):self.s.register_request(RequestIdentity('muse','binding1','generation1','task1','r','a'*64),deadline=20,respond=lambda _:None)
        with self.assertRaises(ValueError):self.s.register_request(RequestIdentity('codex','binding1','generation1','task1','r','bad'),deadline=20,respond=lambda _:None)
        with self.assertRaises(ValueError):self.s.register_request(RequestIdentity('codex','binding1','generation1','task1','r','a'*64),deadline=float('nan'),respond=lambda _:None)
        r=self.request()
        with self.assertRaises(ValueError):self.request()
        with self.assertRaises(ValueError):self.s.acknowledge(r)
    def test_bounds_and_unknown_payloads_fail_closed(self):
        for p in [self.packet(task='x'*65),self.packet(kind='fake_connected'),self.packet(extra='not allowed'),self.packet(sequence=True)]:
            self.assertFalse(self.send(p))
        self.assertEqual(self.s.scene(),'unknown')
        for n in range(16):self.working('task'+str(n))
        self.assertFalse(self.send(self.packet(task='overflow')))
    def test_real_hook_projection_is_provider_specific_and_private(self):
        codex={'hook_event_name':'PreToolUse','session_id':'s1','turn_id':'t1','tool_name':'shell','tool_input':{'command':'private command'}}
        self.assertEqual(project_hook('codex',codex),{'provider':'codex','session':'s1','task':'t1','kind':'working'})
        done=copy.deepcopy(codex);done['hook_event_name']='Stop'
        self.assertEqual(project_hook('codex',done)['kind'],'response_stopping')
        self.assertIsNone(project_hook('muse',codex));self.assertIsNone(project_hook('codex',{'hook_event_name':'PreToolUse'}))
        permission=copy.deepcopy(codex);permission['hook_event_name']='PermissionRequest'
        self.assertIsNone(project_hook('codex',permission)) # must retain actual waiting response owner separately
        self.assertNotIn('private',str(project_hook('codex',codex)))
    def test_native_hook_to_authenticated_event_and_exact_response(self):
        import companion_events as module
        self.assertTrue(hasattr(module,'NativeHookOwner'),'real native hook owner adapter missing')
        owner=module.NativeHookOwner(self.s,'native-session',KEY,lambda packet,signature:self.s.receive(packet,signature))
        payload={'hook_event_name':'UserPromptSubmit','session_id':'native-session','turn_id':'task1'}
        self.assertTrue(owner.observe(payload));self.assertEqual(self.s.scene(),'desk_work')
        prompt={**payload,'hook_event_name':'PermissionRequest','tool_name':'shell','tool_input':{'command':'private operation'}}
        answers=[];identity=owner.open_permission(prompt,request_id='native-request-7',deadline=20.,respond=answers.append)
        self.assertEqual(len(identity.operation_sha256),64)
        self.assertTrue(self.decision(identity));self.assertEqual(answers,[{'hookSpecificOutput':{'hookEventName':'PermissionRequest','decision':{'behavior':'allow'}}}])
        self.assertEqual(self.s.request_state(identity),'submitted')
        self.assertFalse(owner.observe({**payload,'session_id':'other-session'}))
        self.assertNotIn('private',str(self.s.scene()))
    def test_completed_events_release_bounded_task_slots(self):
        for n in range(64):
            task='turn'+str(n);self.working(task)
            self.assertTrue(self.send(self.packet('completed',task)));self.assertEqual(self.s.take_completion(),(task,))

    def test_exact_request_cancel_replace_and_immutable_native_id(self):
        self.working();old=self.request();new=RequestIdentity('codex','binding1','generation1','task1','replacement','b'*64)
        self.s.replace_request(old,new,deadline=20.,respond=lambda choice:self.called.append('new:'+choice))
        self.assertFalse(self.decision(old));self.assertTrue(self.decision(new));self.assertEqual(self.called,['new:approve'])
        with self.assertRaises(ValueError):self.request(digest='c'*64)
        other=self.request('another');self.assertTrue(self.s.cancel_request(other));self.assertFalse(self.decision(other))
    def test_error_acknowledgement_and_completed_slots_do_not_leak(self):
        self.working('failed');self.assertTrue(self.send(self.packet('error','failed')))
        for n in range(32):
            task='after'+str(n);self.working(task);self.assertTrue(self.send(self.packet('completed',task)))
        self.assertEqual(self.s.scene(),'error');self.assertEqual(self.s.take_completion(),())
        self.assertTrue(self.s.acknowledge_error('failed'));self.assertEqual(self.s.scene(),'completion')
        self.assertEqual(len(self.s.take_completion()),32);self.assertEqual(self.s.scene(),'idle_decoration')
    def test_malformed_native_event_is_rejected(self):
        for event in [[],{},7,True,None]:
            self.assertIsNone(project_hook('codex',{'session_id':'s','turn_id':'t','hook_event_name':event}))
    def test_stop_hook_is_not_a_completion_receipt(self):
        self.working();self.assertTrue(self.send(self.packet('response_stopping')))
        self.assertEqual(self.s.scene(),'waiting_receipt');self.assertEqual(self.s.take_completion(),())
        self.assertTrue(self.send(self.packet('completed')));self.assertEqual(self.s.take_completion(),('task1',))

    def test_official_schema_shaped_fixtures_and_distinct_claude_response(self):
        import json
        from pathlib import Path
        import companion_events as module
        cases=json.loads((Path(__file__).parent/'fixtures/provider_native_hooks.json').read_text())['cases']
        for case in cases:
            value=project_hook(case['provider'],case['payload'])
            if case['payload']['hook_event_name']=='PermissionRequest':self.assertIsNone(value)
            else:self.assertEqual(value['provider'],case['provider'])
        session=CompanionSession('claude','b','g',KEY,DECISION_KEY,scopes={'activity','approval'},clock=lambda:10.)
        owner=module.NativeHookOwner(session,'fixture-claude',KEY,session.receive)
        begin=next(c['payload'] for c in cases if c['provider']=='claude' and c['payload']['hook_event_name']=='UserPromptSubmit')
        self.assertTrue(owner.observe(begin))
        request=next(c['payload'] for c in cases if c['provider']=='claude' and c['payload']['hook_event_name']=='PermissionRequest')
        answers=[];identity=owner.open_permission(request,request_id='native-claude-1',deadline=20.,respond=answers.append)
        packet={'version':1,'provider':'claude','binding':'b','generation':'g','sequence':1,'kind':'decision','task':'fixture-turn','request_id':identity.request_id,'operation_sha256':identity.operation_sha256,'choice':'deny'}
        self.assertTrue(session.receive(packet,sign_packet(DECISION_KEY,packet)))
        self.assertEqual(answers,[{'hookSpecificOutput':{'hookEventName':'PermissionRequest','decision':{'behavior':'deny'}}}])

    def test_app_server_receipt_completes_only_bound_thread_and_success(self):
        import companion_events as module
        owner=module.NativeHookOwner(self.s,'thread1',KEY,self.s.receive)
        self.assertTrue(hasattr(owner,'observe_app_server'),'real terminal receipt adapter missing')
        started={'method':'turn/started','params':{'threadId':'thread1','turn':{'id':'task1','status':'inProgress'}}}
        self.assertTrue(owner.observe_app_server(started));self.assertEqual(self.s.scene(),'desk_work')
        failed={'method':'turn/completed','params':{'threadId':'thread1','turn':{'id':'task1','status':'failed'}}}
        self.assertFalse(owner.observe_app_server({**failed,'params':{**failed['params'],'threadId':'foreign'}}))
        self.assertTrue(owner.observe_app_server(failed));self.assertEqual(self.s.scene(),'error');self.assertEqual(self.s.take_completion(),())
        self.s.acknowledge_error('task1')
        next_turn={'method':'turn/started','params':{'threadId':'thread1','turn':{'id':'task2','status':'inProgress'}}}
        self.assertTrue(owner.observe_app_server(next_turn))
        done={'method':'turn/completed','params':{'threadId':'thread1','turn':{'id':'task2','status':'completed'}}}
        self.assertTrue(owner.observe_app_server(done));self.assertEqual(self.s.take_completion(),('task2',))
        self.assertFalse(owner.observe_app_server(done))
    def test_companion_identity_is_distinct_from_native_agent_protocol(self):
        import companion_events as module
        session=CompanionSession('mira','mira-binding','mira-generation',KEY,DECISION_KEY,scopes={'activity'},clock=lambda:10.)
        owner=module.NativeHookOwner(session,'mira-thread',KEY,session.receive,native_protocol='codex')
        self.assertTrue(owner.observe({'session_id':'mira-thread','turn_id':'task1','hook_event_name':'UserPromptSubmit'}))
        self.assertEqual(session.provider,'mira');self.assertEqual(session.scene(),'desk_work')

    def test_hook_only_retirement_is_bounded_and_never_celebrates(self):
        import companion_events as module
        session=CompanionSession('claude','b','g',KEY,DECISION_KEY,scopes={'activity','approval'},clock=lambda:10.)
        owner=module.NativeHookOwner(session,'native',KEY,session.receive)
        self.assertTrue(hasattr(owner,'release_response'),'explicit non-success hook retirement missing')
        for n in range(80):
            task='prompt'+str(n)
            self.assertTrue(owner.observe({'session_id':'native','prompt_id':task,'hook_event_name':'UserPromptSubmit'}))
            self.assertTrue(owner.observe({'session_id':'native','prompt_id':task,'hook_event_name':'Stop'}))
            self.assertTrue(owner.release_response(task));self.assertEqual(session.scene(),'unknown');self.assertEqual(session.take_completion(),())
        self.assertTrue(session.confirm_idle_receipt());self.assertEqual(session.scene(),'idle_decoration')

    def test_actual_app_server_request_retains_native_response_shape(self):
        import companion_events as module
        self.working()
        owner=module.NativeHookOwner(self.s,'thread1',KEY,self.s.receive)
        self.assertTrue(hasattr(owner,'open_app_server_request'),'native App Server approval owner missing')
        request={'id':42,'method':'item/commandExecution/requestApproval','params':{'threadId':'thread1','turnId':'task1','itemId':'item1','command':'fixture-only'}}
        answers=[];identity=owner.open_app_server_request(request,operation_sha256='a'*64,deadline=20.,respond=answers.append)
        self.assertTrue(self.decision(identity));self.assertEqual(answers,[{'decision':'accept'}])
        with self.assertRaises(ValueError):owner.open_app_server_request(request,operation_sha256='b'*64,deadline=20.,respond=answers.append)
        foreign={**request,'id':43,'params':{**request['params'],'threadId':'foreign'}}
        with self.assertRaises(ValueError):owner.open_app_server_request(foreign,operation_sha256='a'*64,deadline=20.,respond=answers.append)
        broad={**request,'id':44,'method':'item/permissions/requestApproval'}
        with self.assertRaises(ValueError):owner.open_app_server_request(broad,operation_sha256='a'*64,deadline=20.,respond=answers.append)

    def test_switch_waits_for_joined_old_owner(self):
        route=ProviderSelection('mira');self.assertFalse(route.select('muse',lambda:False));self.assertEqual(route.selected,'mira')
        self.assertTrue(route.select('muse',lambda:True));self.assertEqual(route.selected,'muse')
        with self.assertRaises(ValueError):route.select('invented',lambda:True)

if __name__=='__main__':unittest.main()
