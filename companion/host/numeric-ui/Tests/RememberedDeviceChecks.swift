import Foundation
final class MockStore: RememberedDeviceStorage {
 var saved: RememberedRobot?; var removed=0; var failRemove=false
 func readForReview() throws -> RememberedRobot? { saved }
 func remove(_ reference: String) throws { if failRemove { throw RememberedDeviceError.unavailable }; removed+=1; saved=nil }
}
final class MockRevoker: DeviceTrustRevoking {
 var calls=0; var fail=false
 func revoke(_ record: RememberedRobot) throws { calls+=1; if fail { throw RememberedDeviceError.unavailable } }
}
@main struct RememberedDeviceChecks {
 static func main() {
  let identity=DurableRobotIdentity(deviceKeyDigest:String(repeating:"a",count:64),hostKeyDigest:String(repeating:"b",count:64),trustProfileDigest:String(repeating:"c",count:64))!
  assert(DurableRobotIdentity(deviceKeyDigest:"001122334455",hostKeyDigest:String(repeating:"b",count:64),trustProfileDigest:String(repeating:"c",count:64))==nil)
  let record=RememberedRobot(identity:identity,secureItemReference:"fixture-reference")!
  let store=MockStore();let revoker=MockRevoker();var model=RememberedPairing(storage:store,revoker:revoker)
  model.requestRemember(true);assert(model.requested && !model.activationAvailable && store.saved==nil)
  model.forget();assert(!model.requested && revoker.calls==0 && store.removed==0)
  store.saved=record;model.loadForReview();assert(model.record==record)
  revoker.fail=true;model.forget();assert(model.record==record && store.removed==0 && model.feedback == .revocationRequired)
  revoker.fail=false;store.failRemove=true;model.forget();assert(model.record==record && model.feedback == .storageCleanupRequired)
  store.failRemove=false;model.forget();assert(model.record==nil && store.removed==1 && model.feedback == .forgotten)
  store.saved=record;model.loadForReview()
  let changed=DurableRobotIdentity(deviceKeyDigest:String(repeating:"d",count:64),hostKeyDigest:identity.hostKeyDigest,trustProfileDigest:identity.trustProfileDigest)!
  assert(model.requiresReenrollment(changed))
  assert(!model.mayAuthorizeSession)
  var disabled=RememberedPairing();disabled.requestRemember(true);disabled.loadForReview()
  assert(disabled.record==nil && !disabled.activationAvailable)
  print("PASS mocked remembered-device identity, opt-in, revoke-before-forget, failure retention and reenrollment; no secure storage/credentials")
 }
}
