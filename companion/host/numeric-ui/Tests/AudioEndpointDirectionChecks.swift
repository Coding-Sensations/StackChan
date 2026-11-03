import Foundation
import CoreAudio

private final class ProvenanceMetadataMock {
 enum Fault: CaseIterable {
  case none, globalError, globalSize, globalMismatch, missingPlugin, bundleError, bundleSize,
       pluginError, pluginSize, pluginMismatch, wrongPluginClass, wrongDeviceClass,
       wrongBundle, wrongUID, wrongTransport, wrongHidden, shortString
 }
 var fault: Fault = .none
 var base: UInt32 = 800
 var ownerReads = 0, deviceListReads = 0
 let plugin: AudioObjectID = 500
 func id(_ uid: String) -> AudioObjectID {
  guard let i = StackChanAudioDevices.definitions.firstIndex(where: { $0.uid == uid }) else { return 0 }
  return base + UInt32(i)
 }
 func read(_ object: AudioObjectID, _ address: UnsafePointer<AudioObjectPropertyAddress>, _ qualifierSize: UInt32, _ qualifier: UnsafeRawPointer?, _ size: UnsafeMutablePointer<UInt32>, _ data: UnsafeMutableRawPointer) -> OSStatus {
  func number(_ n: UInt32) -> OSStatus {
   assert(size.pointee >= 4);data.storeBytes(of:n,as:UInt32.self);size.pointee=4;return noErr
  }
  func string(_ value: String) -> OSStatus {
   assert(size.pointee >= MemoryLayout<Unmanaged<CFString>>.size)
   data.storeBytes(of:Unmanaged.passRetained(value as CFString),as:Unmanaged<CFString>.self)
   size.pointee=UInt32(MemoryLayout<Unmanaged<CFString>>.size)
   if fault == .shortString { size.pointee -= 1 }
   return noErr
  }
  let selector = address.pointee.mSelector
  if selector == kAudioHardwarePropertyTranslateBundleIDToPlugIn ||
     selector == kAudioHardwarePropertyTranslateUIDToDevice {
   assert(qualifierSize == MemoryLayout<CFString>.size && qualifier != nil)
   let text = qualifier!.load(as:CFString.self) as String
   if selector == kAudioHardwarePropertyTranslateBundleIDToPlugIn {
    assert(object == kAudioObjectSystemObject && text == StackChanAudioDevices.bundleID)
    if fault == .bundleError { return -1 }
    let result = number(fault == .missingPlugin ? 0 : plugin)
    if fault == .bundleSize { size.pointee -= 1 };return result
   }
   // The global and plugin UID selectors share their four-char code.
   if object == kAudioObjectSystemObject {
    if fault == .globalError { return -1 }
    let result = number(id(text) + (fault == .globalMismatch ? 1 : 0))
    if fault == .globalSize { size.pointee -= 1 };return result
   }
   assert(object == plugin)
   if fault == .pluginError { return -1 }
   let result = number(id(text) + (fault == .pluginMismatch ? 100 : 0))
   if fault == .pluginSize { size.pointee -= 1 };return result
  }
  if selector == kAudioObjectPropertyOwner { ownerReads += 1; return number(UInt32(kAudioObjectSystemObject)) }
  if selector == kAudioPlugInPropertyDeviceList || selector == kAudioHardwarePropertyDevices {
   deviceListReads += 1;return -1 // hidden endpoints omitted by the real lists
  }
  if selector == kAudioObjectPropertyClass {
   if object == plugin { return number(fault == .wrongPluginClass ? kAudioDeviceClassID : kAudioPlugInClassID) }
   if object == kAudioObjectSystemObject { return number(kAudioSystemObjectClassID) }
   return number(fault == .wrongDeviceClass ? kAudioPlugInClassID : kAudioDeviceClassID)
  }
  if selector == kAudioPlugInPropertyBundleID {
   assert(object == plugin);return string(fault == .wrongBundle ? "another.plugin" : StackChanAudioDevices.bundleID)
  }
  guard let d = StackChanAudioDevices.definitions.first(where: { id($0.uid) == object }) else { return -1 }
  if selector == kAudioDevicePropertyDeviceUID { return string(fault == .wrongUID ? "another.device" : d.uid) }
  if selector == kAudioDevicePropertyTransportType { return number(fault == .wrongTransport ? kAudioDeviceTransportTypeBuiltIn : kAudioDeviceTransportTypeVirtual) }
  if selector == kAudioDevicePropertyIsHidden {
   let hidden = fault == .wrongHidden ? !d.hidden : d.hidden
   return number(hidden ? 1 : 0)
  }
  assertionFailure("Unexpected metadata selector");return -1
 }
}
@main struct AudioEndpointDirectionChecks {
 static func provenanceChecks() {
  let mock = ProvenanceMetadataMock()
  for d in StackChanAudioDevices.definitions {
   assert(StackChanAudioDevices.provenance(mock.id(d.uid),uid:d.uid,hidden:d.hidden,read:mock.read))
  }
  // Freshly renumbered global IDs work; a cached previous ID fails closed.
  let d = StackChanAudioDevices.definitions[0], old = mock.id(StackChanAudioDevices.definitions[0].uid)
  mock.base += 1000
  assert(!StackChanAudioDevices.provenance(old,uid:d.uid,hidden:d.hidden,read:mock.read))
  assert(StackChanAudioDevices.provenance(mock.id(d.uid),uid:d.uid,hidden:d.hidden,read:mock.read))
  for fault in ProvenanceMetadataMock.Fault.allCases where fault != .none {
   mock.fault=fault
   for d in StackChanAudioDevices.definitions {
    assert(!StackChanAudioDevices.provenance(mock.id(d.uid),uid:d.uid,hidden:d.hidden,read:mock.read),"\(fault)")
   }
  }
  mock.fault = .none
  assert(!StackChanAudioDevices.provenance(0,uid:d.uid,hidden:d.hidden,read:mock.read))
  assert(!StackChanAudioDevices.provenance(mock.id(d.uid),uid:"unowned.uid",hidden:false,read:mock.read))
  assert(!StackChanAudioDevices.provenance(mock.id(d.uid),uid:d.uid,hidden:!d.hidden,read:mock.read))
  assert(mock.ownerReads == 0 && mock.deviceListReads == 0)
  var bytes: UInt32 = 8, buffers: UInt32 = 0, readStatus: OSStatus = noErr, sizeStatus: OSStatus = noErr
  let readSize: StackChanAudioDevices.PropertySize = { _,_,_,_,size in size.pointee=bytes;return sizeStatus }
  let read: StackChanAudioDevices.PropertyRead = { _,_,_,_,size,data in
   data.storeBytes(of:buffers,as:UInt32.self);return readStatus
  }
  assert(StackChanAudioDevices.channels(1,kAudioObjectPropertyScopeInput,read:read,readSize:readSize)==0)
  buffers=2
  assert(StackChanAudioDevices.channels(1,kAudioObjectPropertyScopeInput,read:read,readSize:readSize)==(-1))
  buffers=0;readStatus = -1
  assert(StackChanAudioDevices.channels(1,kAudioObjectPropertyScopeInput,read:read,readSize:readSize)==(-1))
  readStatus=noErr;sizeStatus = -1
  assert(StackChanAudioDevices.channels(1,kAudioObjectPropertyScopeInput,read:read,readSize:readSize)==(-1))
  sizeStatus=noErr;bytes=65537
  assert(StackChanAudioDevices.channels(1,kAudioObjectPropertyScopeInput,read:read,readSize:readSize)==(-1))
  bytes=3
  assert(StackChanAudioDevices.streams(1,kAudioObjectPropertyScopeInput,readSize:readSize)==(-1))
 }
 static func main() {
  let microphone = AudioEndpointDirection.microphone, speaker = AudioEndpointDirection.speaker
  // Actual previously exposed aggregate topology: rejected for BOTH public roles.
  assert(!microphone.accepts(inputChannels:2,outputChannels:2,inputStreams:1,outputStreams:1))
  assert(!speaker.accepts(inputChannels:2,outputChannels:2,inputStreams:1,outputStreams:1))
  assert(microphone.accepts(inputChannels:2,outputChannels:0,inputStreams:1,outputStreams:0))
  assert(speaker.accepts(inputChannels:0,outputChannels:2,inputStreams:0,outputStreams:1))
  assert(!microphone.accepts(inputChannels:0,outputChannels:2,inputStreams:0,outputStreams:1))
  assert(!speaker.accepts(inputChannels:2,outputChannels:0,inputStreams:1,outputStreams:0))
  // Channel counts alone cannot hide a remaining opposite-direction stream.
  assert(!microphone.accepts(inputChannels:2,outputChannels:0,inputStreams:1,outputStreams:1))
  assert(!speaker.accepts(inputChannels:0,outputChannels:2,inputStreams:1,outputStreams:1))
  for invalid in [-1,0,1,3] { assert(!microphone.accepts(inputChannels:invalid,outputChannels:0,inputStreams:1,outputStreams:0)) }
  assert(!speaker.accepts(inputChannels:0,outputChannels:2,inputStreams:0,outputStreams:-1))
  let observed=[(2,2,1,1),(2,0,1,0),(0,2,0,1)]
  let inputs=observed.indices.filter { let x=observed[$0];return microphone.accepts(inputChannels:x.0,outputChannels:x.1,inputStreams:x.2,outputStreams:x.3) }
  let outputs=observed.indices.filter { let x=observed[$0];return speaker.accepts(inputChannels:x.0,outputChannels:x.1,inputStreams:x.2,outputStreams:x.3) }
  assert(inputs == [1] && outputs == [2])
  let current = Array(DirectionalAudioCatalog.publicUIDs)
  assert(DirectionalAudioCatalog.accepts(current))
  assert(DirectionalAudioCatalog.accepts(current + ["unrelated.device"]))
  assert(!DirectionalAudioCatalog.accepts(nil)) // enumeration failure is unknown
  assert(!DirectionalAudioCatalog.accepts([]))
  assert(!DirectionalAudioCatalog.accepts([current[0]]))
  for forbidden in DirectionalAudioCatalog.forbiddenVisibleUIDs {
   assert(!DirectionalAudioCatalog.accepts(current + [forbidden]))
  }
  let route = AudioDeviceRoute(endpointsAvailable: true).workerPayload()
  let audio = route?["audio_route"] as? [String: String]
  assert(audio?["input_uid"] == "local.mrkillbob.StackChan.hal.microphone.v2")
  assert(audio?["output_uid"] == "local.mrkillbob.StackChan.hal.speaker.v2")
  assert(AudioDeviceRoute(endpointsAvailable: false).workerPayload() == nil)
  provenanceChecks()
  print("PASS public direction/enumeration gates; duplex aliases rejected; no audio IO")
 }
}
