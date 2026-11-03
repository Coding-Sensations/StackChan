import Foundation
@main struct PersistentChecks {
    static func pump(_ seconds: Double = 3, _ condition: () -> Bool) {
        let deadline=Date().addingTimeInterval(seconds)
        while !condition() && Date()<deadline { RunLoop.current.run(until:Date().addingTimeInterval(0.02)) }
        precondition(condition(), "bounded fixture did not complete")
    }
    static func fake(_ source: String) -> Process {
        let p=Process();p.executableURL=URL(fileURLWithPath:ProcessInfo.processInfo.environment["STACKCHAN_TEST_PYTHON"] ?? "/usr/bin/python3");p.arguments=["-B","-u","-c",FixtureCustody.wrap(source)];return p
    }
    static func main() {
        let root=URL(fileURLWithPath:"/synthetic")
        let controller=PairingController(approved:false,root:root)
        precondition(controller.workerLaunchArguments.contains("--persistent-trust"))
        precondition(controller.workerLaunchArguments.contains(where:{$0.hasSuffix("persistent-companion/server.pem")}))
        precondition(!controller.canConnectRemembered)
        precondition(controller.canEnrollRemembered)
        var model = AudioDeviceRoute(endpointsAvailable: false)
        precondition(!model.canUseStackChan)
        model = AudioDeviceRoute(endpointsAvailable: true)
        precondition(model.canUseStackChan)
        let payload = model.workerPayload()
        precondition(payload?["command"] as? String == "connect")
        let route = payload?["audio_route"] as? [String: Any]
        precondition(route?["input_uid"] as? String == AudioDeviceRoute.voiceInputUID)
        precondition(route?["output_uid"] as? String == AudioDeviceRoute.voiceOutputUID)
        precondition(route?.count == 2)
        precondition(!controller.rememberedWorkerLaunchArguments.joined().contains("credential"))
        precondition(controller.rememberedWorkerLaunchArguments.contains(where:{$0.hasSuffix("remembered_pairing_worker.py")}))
        let audio=PairingController(approved:true,root:root,audioRouteAvailability:{ true })
        audio.refreshAudioRouteAvailability()
        precondition(audio.canConnectRemembered)
        let enrolled = PairingController(approved:true,root:root,processFactory:{fake("""
import sys,json
for line in sys.stdin:
 message=json.loads(line);c=message['command']
 if c=='enroll':print(json.dumps({'state':'paired','acknowledged':True}),flush=True)
 elif c=='connect':
  assert set(message['audio_route']) == {'input_uid','output_uid'}
  print(json.dumps({'state':'paired','acknowledged':True,'activity':'physical_consent'}),flush=True)
  import time;time.sleep(.05)
  print(json.dumps({'state':'paired','acknowledged':True,'activity':'conversation','audio_remaining':60}),flush=True)
 elif c=='cancel':print(json.dumps({'state':'cancelled'}),flush=True)
""")},audioRouteAvailability:{ true })
        enrolled.enrollRemembered();pump { enrolled.phase == .paired }
        enrolled.refreshAudioRouteAvailability()
        precondition(enrolled.canUseStackChan)
        enrolled.useStackChan();pump { enrolled.activity == .physicalConsent }
        pump { enrolled.activity == .conversation }
        precondition(enrolled.rememberedStatus.localizedCaseInsensitiveContains("conversation activity") &&
                     enrolled.rememberedStatus.localizedCaseInsensitiveContains("capture state is unknown") &&
                     !enrolled.rememberedStatus.localizedCaseInsensitiveContains("press listen"),
                     "remembered-device status cannot claim audio remains muted after conversation activity")
        precondition(!enrolled.canUseStackChan)
        enrolled.cancel();pump { enrolled.phase == .cancelled }
        precondition(enrolled.rememberedStatus.localizedCaseInsensitiveContains("release is not confirmed") &&
                     enrolled.rememberedStatus.localizedCaseInsensitiveContains("capture state is unknown") &&
                     !enrolled.rememberedStatus.localizedCaseInsensitiveContains("audio is closed"),
                     "cancelled worker status cannot imply process release or known capture state")
        var done=false;enrolled.shutdown { done=$0 };pump { done }
        precondition(enrolled.rememberedStatus.localizedCaseInsensitiveContains("process release is confirmed"),
                     "remembered-device status advances only after confirmed process exit")
        let retry = PairingController(approved:true,root:root,processFactory:{fake("""
import sys,json
count=0
for line in sys.stdin:
 c=json.loads(line)['command']
 if c=='forget':
  count+=1
  if count==1:print(json.dumps({'state':'failed','message':'cleanup_unconfirmed','cleanup':['grant']}),flush=True)
  elif count==2:print(json.dumps({'state':'cancelled'}),flush=True)
  else:print(json.dumps({'state':'cancelled','message':'remembered_forgotten'}),flush=True)
 elif c=='cancel':print(json.dumps({'state':'cancelled'}),flush=True)
""")},audioApproved:true,audioMode:.nativeConversation)
        retry.forgetRemembered();pump { retry.cleanupIssues.contains(.grant) }
        precondition(retry.canForgetRemembered && !retry.canEnrollRemembered && !retry.canConnectRemembered)
        retry.forgetRemembered();pump { retry.phase == .cancelled }
        precondition(retry.cleanupIssues.contains(.grant) && retry.cleanupPending && !retry.canConnectRemembered)
        retry.forgetRemembered();pump { retry.cleanupIssues.isEmpty && retry.phase == .cancelled }
        precondition(!retry.cleanupPending)
        done=false;retry.shutdown { done=$0 };pump { done }
        let audioFault = PairingController(approved:true,root:root,processFactory:{fake("""
import sys,json
for line in sys.stdin:
 c=json.loads(line)['command']
 if c=='forget':print(json.dumps({'state':'failed','message':'cleanup_unconfirmed','cleanup':['audio','grant']}),flush=True)
 elif c=='cancel':print(json.dumps({'state':'cancelled'}),flush=True)
""")},audioApproved:true,audioMode:.nativeConversation)
        audioFault.forgetRemembered();pump { audioFault.cleanupIssues.contains(.audio) }
        precondition(!audioFault.canForgetRemembered && !audioFault.canConnectRemembered && !audioFault.canEnrollRemembered)
        done=false;var reported=false;audioFault.shutdown { done=$0;reported=true };pump { reported };precondition(!done && audioFault.cleanupIssues.contains(.audio))
        let retained = PairingController(approved:true,root:root,processFactory:{fake("""
import sys,json,signal,time
signal.signal(signal.SIGTERM,lambda *args:None)
for line in sys.stdin:
 c=json.loads(line)['command']
 if c=='enroll':print(json.dumps({'state':'paired','acknowledged':True}),flush=True)
 elif c=='cancel':
  print(json.dumps({'state':'failed','message':'cleanup_unconfirmed','cleanup':['audio']}),flush=True)
  time.sleep(7)
  print(json.dumps({'state':'closed','message':'remembered_cleanup_verified'}),flush=True)
  break
""")},audioApproved:true,audioMode:.nativeConversation)
        retained.enrollRemembered();pump { retained.phase == .paired }
        done=false;retained.shutdown { done=$0 }
        pump(12) { done }
        precondition(retained.phase == .closed && retained.cleanupIssues.isEmpty, "persistent owner was killed before verified cleanup")
        // Real inert import check with the same environment used by the worker.
        // Imports need the same configured project/dependency root as the real worker.
        let candidateRoot = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let importRoot = ProcessInfo.processInfo.environment["STACKCHAN_FIXTURE_PROJECT_ROOT"].map { URL(fileURLWithPath: $0) } ?? candidateRoot
        precondition(FileManager.default.fileExists(atPath: importRoot.appendingPathComponent("bridge/host-adapter/host_adapter.py").path), "fixture project dependency root required")
        let runtime = PairingController(approved: false, root: importRoot)
        let probe=Process();probe.executableURL=URL(fileURLWithPath:ProcessInfo.processInfo.environment["STACKCHAN_TEST_PYTHON"] ?? "/usr/bin/python3")
        probe.arguments=["-B","-c",FixtureCustody.wrap("import native_adapter, native_conversation")]
        probe.currentDirectoryURL=URL(fileURLWithPath:runtime.rememberedWorkerLaunchArguments.last!).deletingLastPathComponent()
        probe.environment=runtime.workerLaunchEnvironment
        try! probe.run();probe.waitUntilExit()
        precondition(probe.terminationStatus == 0, "worker environment cannot import the inert native audio adapter")
        print("Persistent app command and route gates passed")
    }
}
