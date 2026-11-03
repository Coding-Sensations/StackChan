import Foundation
import Darwin
@main struct LifecycleChecks {
 static func pump(_ seconds: Double = 3, line: UInt = #line, _ condition: () -> Bool) {
  let deadline = Date().addingTimeInterval(seconds)
  while !condition() && Date() < deadline { RunLoop.current.run(until: Date().addingTimeInterval(0.02)) }
  precondition(condition(), "bounded fixture did not complete at line \(line)")
 }
 static func fake(_ code: String) -> Process {
  let p=Process();p.executableURL=URL(fileURLWithPath:ProcessInfo.processInfo.environment["STACKCHAN_TEST_PYTHON"] ?? "/usr/bin/python3")
  p.arguments=["-B","-u","-c",FixtureCustody.wrap(code)]
  return p
 }
 static func main() {
  let root=URL(fileURLWithPath:"/tmp")
  let inert=PairingController(approved:false,root:root)
  assert(!inert.canPrimary && !inert.canCancel && !inert.canConfirm && !inert.audioApproved)
  let scoped=PairingController(approved:true,root:root,audioApproved:true)
  assert(scoped.audioApproved)
  let denied=PairingController(approved:false,root:root,audioApproved:true)
  assert(!denied.audioApproved)
  let native = PairingController(approved:true,root:root,audioApproved:true,audioMode:.nativeConversation)
  assert(native.audioDisclosure.contains("original dot voice call"))
  assert(native.audioDisclosure.contains("call's audio plays on StackChan"))
  assert(!native.audioDisclosure.contains("discarded"))
  assert(native.audioSubtitle.contains("Original dot call"))
  assert(Array(native.workerLaunchArguments.suffix(3)) == ["--approved-audio","--audio-mode","native_conversation"])
  assert(scoped.audioDisclosure.contains("counted and discarded locally"))
  assert(scoped.audioDisclosure.contains("No audio recording or forwarding"))
  assert(Array(scoped.workerLaunchArguments.suffix(3)) == ["--approved-audio","--audio-mode","transport_test"])
  assert(!inert.workerLaunchArguments.contains("--approved-audio"))
  let normal="""
import sys,json
for line in sys.stdin:
 c=json.loads(line)['command']
 if c in ('prepare','retry'):print(json.dumps({'state':'comparing','code':'012345','remaining':120}),flush=True)
 elif c=='confirm':print(json.dumps({'state':'paired','acknowledged':True}),flush=True)
 elif c=='cancel':print(json.dumps({'state':'cancelled'}),flush=True)
"""
  let a=PairingController(approved:true,root:root,processFactory:{fake(normal)},oneAttempt:false)
  a.primary();assert(!a.canPrimary);pump{a.phase == .comparing};assert(a.code=="012345" && a.canConfirm)
  assert(a.secondsRemaining(at:ProcessInfo.processInfo.systemUptime+121)==0)
  a.primary();assert(a.confirmationSent && !a.canConfirm);pump{a.phase == .paired};assert(!a.canPrimary)
  a.cancel();assert(a.cleanupPending && !a.canPrimary);pump{a.phase == .cancelled};assert(a.canPrimary)
  a.primary();pump{a.phase == .comparing};assert(!a.confirmationSent)
  var stopped=false;a.shutdown{closed in stopped=closed};pump{stopped}
  let audio=PairingController(approved:true,root:root,processFactory:{fake(normal)},audioApproved:true)
  audio.primary();pump{audio.phase == .comparing}
  assert(audio.primaryTitle == "Confirm & connect")
  assert(audio.instruction.contains("temporary") && audio.instruction.contains("microphone") && audio.instruction.contains("speaker"))
  audio.primary();pump{audio.phase == .paired}
  assert(!audio.instruction.contains("tap Connect") && !audio.instruction.contains("then Allow"))
  var audioStopped=false;audio.shutdown{closed in audioStopped=closed};pump{audioStopped}
  let single=PairingController(approved:true,root:root,processFactory:{fake(normal)})
  single.primary();pump{single.phase == .comparing};single.cancel();pump{single.phase == .cancelled}
  assert(!single.canPrimary && single.primaryTitle == "New approval needed")
  single.primary();assert(single.phase == .cancelled)
  var singleStopped=false;single.shutdown{closed in singleStopped=closed};pump{singleStopped}
  let cleanup="""
import sys,json
sys.stdin.readline()
print(json.dumps({'state':'failed','message':'cleanup_unconfirmed','cleanup':['audio','grant','usb']}),flush=True)
"""
  let blocked=PairingController(approved:true,root:root,processFactory:{fake(cleanup)},oneAttempt:false)
  blocked.primary();pump{blocked.phase == .failed && blocked.cleanupIssues == [.audio,.grant,.usb]}
  assert(!blocked.canPrimary && blocked.primaryTitle == "Cleanup needed")
  assert(blocked.instruction.contains("Do not try again") && blocked.instruction.contains("microphone and speaker routes are restored"))
  assert(blocked.instruction.contains("pairing grant was revoked") && blocked.instruction.contains("USB pairing session is closed"))
  var blockedStopped:Bool?=nil;blocked.shutdown{closed in blockedStopped=closed};pump{blockedStopped != nil};assert(blockedStopped == false)
  // A worker that prints its final cleanup report and exits immediately races the controller's
  // pipe read against process termination; repeat to cover both orderings. The report must never be
  // lost, because a lost report would look like a clean exit-0 release.
  for _ in 0..<25 {
   let racing=PairingController(approved:true,root:root,processFactory:{fake(cleanup)},oneAttempt:false)
   racing.primary();pump{racing.phase == .failed && racing.cleanupIssues == [.audio,.grant,.usb] && racing.cleanupPending}
  }
  // PipeReader is the only read path: nonblocking for life, bounded, explicit outcomes, no process or sleep.
  do {
   var fds:[Int32]=[0,0];precondition(Darwin.pipe(&fds)==0);let rd=fds[0],wr=fds[1]
   let reader=PipeReader(fd:rd,limit:1000)!
   var got=Data()
   // Writer still open and nothing to read (a descendant holding the pipe): an outcome, not a block or raise.
   assert(reader.read{got.append($0)} == .wouldBlock && got.isEmpty)
   // Bounded: a call never returns more than the limit; the rest stays for the next readability event.
   let blob=[UInt8](repeating:65,count:3000);assert(Darwin.write(wr,blob,3000)==3000)
   assert(reader.read{got.append($0)} == .data && got.count==1000)
   assert(reader.read{got.append($0)} == .data && got.count==2000)
   assert(reader.read{got.append($0)} == .data && got.count==3000)
   assert(reader.read{got.append($0)} == .wouldBlock && got.count==3000)
   // Retirement drains once and later (already queued) reads take nothing, even if the writer is still open.
   assert(Darwin.write(wr,"tail\n",5)==5)
   var remainder=Data();var retiredFaulted=true;reader.retire{remainder=$0.data;retiredFaulted=$0.faulted};assert(String(decoding:remainder,as:UTF8.self)=="tail\n" && !retiredFaulted)
   assert(Darwin.write(wr,"late",4)==4)
   assert(reader.read{got.append($0)} == .retired && got.count==3000)
   var late=[UInt8](repeating:0,count:8);assert(Darwin.read(rd,&late,8)==4,"retired reader must leave later bytes unread")
   var again=Data([1]);reader.retire{again=$0.data};assert(again.isEmpty)
   Darwin.close(wr);Darwin.close(rd)
   // EOF once every writer is gone.
   precondition(Darwin.pipe(&fds)==0)
   let eofReader=PipeReader(fd:fds[0])!;Darwin.close(fds[1])
   assert(eofReader.read{_ in} == .eof);Darwin.close(fds[0])
   // A hard read error is reported, not raised (a directory descriptor fails with EISDIR).
   let dir=Darwin.open("/",O_RDONLY);precondition(dir >= 0)
   let broken=PipeReader(fd:dir)!
   assert(broken.read{_ in} == .failed(EISDIR));Darwin.close(dir)
   // A read error is sticky and survives retirement: the controller must not call that a clean release.
   var brokenFault=false;broken.retire{brokenFault=$0.faulted};assert(brokenFault)
   // A final drain that stops at the byte bound is incomplete, not a clean EOF.
   precondition(Darwin.pipe(&fds)==0)
   let small=PipeReader(fd:fds[0],limit:10)!;assert(Darwin.write(fds[1],[UInt8](repeating:66,count:30),30)==30)
   var boundFault=false,boundBytes=0;small.retire{boundFault=$0.faulted;boundBytes=$0.data.count}
   assert(boundFault && boundBytes == 10);Darwin.close(fds[0]);Darwin.close(fds[1])
  }
  // The release verdict needs a clean exit, no reported cleanup issue AND a fault-free read of the final output.
  assert(PairingController.releaseVerdict(exitedCleanly:true,cleanupIssuesEmpty:true,readFaulted:false))
  assert(!PairingController.releaseVerdict(exitedCleanly:true,cleanupIssuesEmpty:true,readFaulted:true))
  assert(!PairingController.releaseVerdict(exitedCleanly:true,cleanupIssuesEmpty:false,readFaulted:false))
  assert(!PairingController.releaseVerdict(exitedCleanly:false,cleanupIssuesEmpty:true,readFaulted:false))
  // A worker that leaves a descendant holding the output pipe open. Finalization (final report read, exit
  // processed, shutdown completed) must happen while the descendant still holds the pipe, so it cannot depend
  // on EOF. Synchronization is by files, not sleeps; the descendant owns a 6 s self-expiry so it cannot outlive
  // a failed run, and the test releases and reaps it on every path before asserting.
  func heldPipeResult() -> Bool {
   let dir=NSTemporaryDirectory()+"h01-held-\(UUID().uuidString)"
   try! FileManager.default.createDirectory(atPath:dir,withIntermediateDirectories:true)
   let pidFile=dir+"/pid", releaseFile=dir+"/release"
   var descendant:pid_t=0
   defer {
    FileManager.default.createFile(atPath:releaseFile,contents:Data())
    let end=Date().addingTimeInterval(3)
    while descendant > 0 && Darwin.kill(descendant,0) == 0 && Date() < end {
     RunLoop.current.run(until:Date().addingTimeInterval(0.02))
    }
    if descendant > 0 && Darwin.kill(descendant,0) == 0 { Darwin.kill(descendant,SIGKILL) }
    try? FileManager.default.removeItem(atPath:dir)
   }
   let held="""
import sys,json,subprocess,os,time
sys.stdin.readline()
child='''import os,sys,time
open(sys.argv[1]+'.tmp','w').write(str(os.getpid()))
os.rename(sys.argv[1]+'.tmp',sys.argv[1])
end=time.time()+6
while not os.path.exists(sys.argv[2]) and time.time()<end:time.sleep(0.02)
'''
subprocess.Popen([sys.executable,'-B','-c',child,'\(pidFile)','\(releaseFile)'])
end=time.time()+2
while not os.path.exists('\(pidFile)') and time.time()<end:time.sleep(0.02)
print(json.dumps({'state':'failed','message':'cleanup_unconfirmed','cleanup':['audio']}),flush=True)
"""
   let ctl=PairingController(approved:true,root:root,processFactory:{fake(held)},oneAttempt:false)
   var result:Bool?=nil
   ctl.primary()
   let end=Date().addingTimeInterval(3)
   while !(ctl.phase == .failed && ctl.cleanupIssues == [.audio]) && Date() < end {
    RunLoop.current.run(until:Date().addingTimeInterval(0.02))
   }
   ctl.shutdown{result=$0}
   let stopEnd=Date().addingTimeInterval(2)
   while result == nil && Date() < stopEnd { RunLoop.current.run(until:Date().addingTimeInterval(0.02)) }
   descendant=pid_t((try? String(contentsOfFile:pidFile,encoding:.utf8)).flatMap{Int32($0)} ?? 0)
   return result == false && ctl.cleanupPending && ctl.cleanupIssues == [.audio]
       && descendant > 0 && Darwin.kill(descendant,0) == 0
       && !FileManager.default.fileExists(atPath:releaseFile)
  }
  let heldPipePassed=heldPipeResult()
  assert(heldPipePassed,"finalization must complete before descendant EOF; cleanup precedes assertion")
  // The 8 s deadline must not confirm a release while exit/final-output finalization is still pending.
  // The gate holds every pipe read and the termination handler, so the worker has exited 0 and its cleanup
  // report sits unread when the (shortened) deadline fires. Old behavior reported a clean release here.
  let pendingWorker="""
import sys,json
sys.stdin.readline();print(json.dumps({'state':'waiting_robot'}),flush=True)
sys.stdin.readline();print(json.dumps({'state':'failed','message':'cleanup_unconfirmed','cleanup':['audio']}),flush=True)
"""
  let gateWorker=fake(pendingWorker)
  let gateCtl=PairingController(approved:true,root:root,processFactory:{gateWorker})
  let gate=DispatchSemaphore(value:0);gateCtl.finalizationGate=gate
  gateCtl.escalationDelays=(terminate:30,kill:30,deadline:0.6)
  gateCtl.primary()
  gate.signal();pump{gateCtl.phase == .waiting_robot}          // let the first status through, then close the gate
  _=gate.wait(timeout:.now()+1)
  var deadlineResults=[Bool](),deadlineExitStates=[Bool]();gateCtl.shutdown{deadlineResults.append($0);deadlineExitStates.append(!gateWorker.isRunning)}
  pump{!gateWorker.isRunning}
  pump{deadlineResults.count == 1}
  assert(deadlineResults == [false] && deadlineExitStates == [true] && gateCtl.cleanupPending && gateCtl.cleanupIssues.isEmpty,"deadline must not confirm while finalization is pending")
  gate.signal()                                                // open the gate: ordered finalization now runs
  pump{gateCtl.cleanupIssues == [.audio]}
  assert(gateCtl.cleanupPending && deadlineResults.count == 1)
  // A malformed final line printed while Stop is in progress: every completion runs once, no hang.
  let stopMalformed="""
import sys,json
sys.stdin.readline();print(json.dumps({'state':'waiting_robot'}),flush=True)
sys.stdin.readline();print('not json',flush=True)
"""
  let mal=PairingController(approved:true,root:root,processFactory:{fake(stopMalformed)})
  mal.primary();pump{mal.phase == .waiting_robot}
  var malResults=[Bool]();mal.shutdown{malResults.append($0)};mal.shutdown{malResults.append($0)}
  pump{malResults.count >= 2}
  // Preserved contract: malformed output marks the attempt failed (protocolFailure), but a clean exit 0 with no
  // reported cleanup issue and a fault-free read is still accepted as a confirmed release.
  assert(malResults == [true,true] && mal.phase == .failed && mal.recovery == .protocolFailure && !mal.cleanupPending && mal.escalationSchedules == 1)
  // Repeated shutdown after the worker closed its stdin: the cancel hits EPIPE, the requests coalesce,
  // every completion runs once, and escalation is scheduled once.
  let closedStdin="""
import sys,json,os,time
sys.stdin.readline();os.close(sys.stdin.fileno());print(json.dumps({'state':'waiting_robot'}),flush=True)
time.sleep(1)
"""
  let epipe=PairingController(approved:true,root:root,processFactory:{fake(closedStdin)})
  epipe.primary();pump{epipe.phase == .waiting_robot}
  var ep=[Bool]();epipe.shutdown{ep.append($0)};epipe.shutdown{ep.append($0)};epipe.shutdown{ep.append($0)}
  pump{ep.count == 3};assert(ep.allSatisfy{$0 == ep[0]} && epipe.recovery == .connection && epipe.escalationSchedules == 1)
  var afterExit=0;epipe.shutdown{_ in afterExit+=1};assert(afterExit == 1)
  let invalid="""
import sys,json,time,os
sys.stdin.readline()
os.write(1,(json.dumps({'state':'failed','credential':'synthetic-no-authority'})+'\\n'+json.dumps({'state':'cancelled'})+'\\n').encode())
for line in sys.stdin:pass
time.sleep(1)
"""
  let b=PairingController(approved:true,root:root,processFactory:{fake(invalid)},oneAttempt:false)
  b.primary();pump{b.phase == .failed && b.cleanupPending};assert(!b.canPrimary)
  var invalidStopped:Bool?=nil;b.shutdown{invalidStopped=$0};pump{invalidStopped != nil}
  assert(invalidStopped == false && !b.canPrimary,"unprocessed output must not imply cleanup")
  var count=0
  let partial="""
import sys
sys.stdin.readline()
sys.stdout.write('{"state":"comparing",');sys.stdout.flush()
"""
  let c=PairingController(approved:true,root:root,processFactory:{count+=1;return fake(count==1 ? partial : normal)},oneAttempt:false)
  c.primary();pump{c.phase == .failed && c.cleanupPending && c.recovery == .protocolFailure}
  var partialStopped:Bool?=nil;c.shutdown{partialStopped=$0};pump{partialStopped != nil}
  assert(partialStopped == false && !c.canPrimary,"truncated status must not imply a clean release")
  let stubborn="""
import sys,signal,time,json
signal.signal(signal.SIGTERM,signal.SIG_IGN)
sys.stdin.readline();print(json.dumps({'state':'waiting_robot'}),flush=True)
while True:time.sleep(0.1)
"""
  let d=PairingController(approved:true,root:root,processFactory:{fake(stubborn)})
  d.primary();pump{d.phase == .waiting_robot};var stubbornClosed:Bool?=nil;d.shutdown{closed in stubbornClosed=closed}
  pump(9){stubbornClosed != nil};assert(stubbornClosed == false && d.cleanupPending,"forced worker kill is not a confirmed clean release")
  assert(d.escalationSchedules == 1 && malResults.count == 2 && ep.count == 3 && afterExit == 1 && heldPipePassed && deadlineResults.count == 1,"no completion may run twice")
  print("PASS nativeUI real-pipe fixtures: preview, repeatedclick, confirm, cancel/retry, malformedstatus cleanup, stalepartial isolation, EOF closure and TERM-resistant workerKILL; nohardware/network")
 }
}
