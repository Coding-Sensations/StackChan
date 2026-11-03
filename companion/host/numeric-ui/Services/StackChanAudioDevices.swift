import CoreAudio
import Foundation

/// Inspect public endpoint directions without changing the internal transport buses.
enum StackChanAudioDevices {
    typealias PropertyRead = (AudioObjectID, UnsafePointer<AudioObjectPropertyAddress>, UInt32, UnsafeRawPointer?, UnsafeMutablePointer<UInt32>, UnsafeMutableRawPointer) -> OSStatus
    typealias PropertySize = (AudioObjectID, UnsafePointer<AudioObjectPropertyAddress>, UInt32, UnsafeRawPointer?, UnsafeMutablePointer<UInt32>) -> OSStatus
    static let systemRead: PropertyRead = { object, address, qualifierSize, qualifier, size, data in
        AudioObjectGetPropertyData(object, address, qualifierSize, qualifier, size, data)
    }
    static let systemSize: PropertySize = { object, address, qualifierSize, qualifier, size in
        AudioObjectGetPropertyDataSize(object, address, qualifierSize, qualifier, size)
    }
    static let bundleID = "local.mrkillbob.StackChan.DirectionalAudio"
    static let definitions: [(name: String, uid: String, direction: AudioEndpointDirection, hidden: Bool)] = [
        ("StackChan Microphone", "local.mrkillbob.StackChan.hal.microphone.v2", .microphone, false),
        ("StackChan Speaker", "local.mrkillbob.StackChan.hal.speaker.v2", .speaker, false),
        ("StackChan Internal Feed", "local.mrkillbob.StackChan.hal.feed.v2", .speaker, true),
        ("StackChan Internal Return", "local.mrkillbob.StackChan.hal.return.v2", .microphone, true)
    ]
    enum Failure: Error { case wrongTopology, directionalDeviceRequired, legacyAliasesStillVisible }
    static func string(_ device: AudioObjectID, _ selector: AudioObjectPropertySelector, read: PropertyRead = systemRead) -> String? {
        var a = AudioObjectPropertyAddress(mSelector: selector, mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMain)
        var v: Unmanaged<CFString>?; var n = UInt32(MemoryLayout.size(ofValue: v))
        let status = read(device, &a, 0, nil, &n, &v)
        defer { v?.release() }
        guard status == noErr, n == MemoryLayout.size(ofValue: v),
              let value = v?.takeUnretainedValue(), CFGetTypeID(value) == CFStringGetTypeID() else { return nil }
        return value as String
    }
    static func devices() -> [AudioObjectID]? {
        var a = AudioObjectPropertyAddress(mSelector: kAudioHardwarePropertyDevices, mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMain)
        var n: UInt32 = 0
        guard AudioObjectGetPropertyDataSize(AudioObjectID(kAudioObjectSystemObject), &a, 0, nil, &n) == noErr,
              n % UInt32(MemoryLayout<AudioObjectID>.size) == 0 else { return nil }
        if n == 0 { return [] }
        var ids = [AudioObjectID](repeating: 0, count: Int(n) / MemoryLayout<AudioObjectID>.size)
        let expected = n
        guard AudioObjectGetPropertyData(AudioObjectID(kAudioObjectSystemObject), &a, 0, nil, &n, &ids) == noErr, n == expected else { return nil }
        return ids
    }
    static func translate(_ object: AudioObjectID, _ selector: AudioObjectPropertySelector, _ text: String, read: PropertyRead = systemRead) -> AudioObjectID? {
        var a = AudioObjectPropertyAddress(mSelector: selector, mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMain)
        var value = text as CFString; var id = AudioObjectID(kAudioObjectUnknown)
        var n = UInt32(MemoryLayout.size(ofValue: id))
        let qualifierSize = UInt32(MemoryLayout<CFString>.size)
        let status = withUnsafePointer(to: &value) { qualifier in
            read(object, &a, qualifierSize, qualifier, &n, &id)
        }
        return status == noErr && n == MemoryLayout<AudioObjectID>.size && id != kAudioObjectUnknown ? id : nil
    }
    static func find(_ uid: String) -> AudioObjectID? {
        // Hidden endpoints are absent from visibility-filtered device lists.
        translate(AudioObjectID(kAudioObjectSystemObject), kAudioHardwarePropertyTranslateUIDToDevice, uid)
    }
    static func number(_ id: AudioObjectID, _ selector: AudioObjectPropertySelector, _ scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal, read: PropertyRead = systemRead) -> UInt32? {
        var a = AudioObjectPropertyAddress(mSelector: selector, mScope: scope, mElement: kAudioObjectPropertyElementMain)
        var value: UInt32 = 0; var n = UInt32(MemoryLayout.size(ofValue: value))
        guard read(id, &a, 0, nil, &n, &value) == noErr, n == MemoryLayout<UInt32>.size else { return nil }
        return value
    }
    static func channels(_ id: AudioObjectID, _ scope: AudioObjectPropertyScope, read: PropertyRead = systemRead, readSize: PropertySize = systemSize) -> Int {
        var a = AudioObjectPropertyAddress(mSelector: kAudioDevicePropertyStreamConfiguration, mScope: scope, mElement: kAudioObjectPropertyElementMain)
        var n: UInt32 = 0
        guard readSize(id, &a, 0, nil, &n) == noErr, n >= MemoryLayout<UInt32>.size, n <= 65536 else { return -1 }
        let requested = n
        let allocated = max(Int(n), MemoryLayout<AudioBufferList>.size)
        let p = UnsafeMutableRawPointer.allocate(byteCount: allocated, alignment: MemoryLayout<AudioBufferList>.alignment); defer { p.deallocate() }
        p.initializeMemory(as: UInt8.self, repeating: 0, count: allocated)
        guard read(id, &a, 0, nil, &n, p) == noErr, n <= requested, n >= MemoryLayout<UInt32>.size else { return -1 }
        let list = p.assumingMemoryBound(to: AudioBufferList.self)
        let count = Int(list.pointee.mNumberBuffers)
        if count == 0 { return 0 }
        guard let offset = MemoryLayout<AudioBufferList>.offset(of: \.mBuffers),
              Int(n) >= offset + count * MemoryLayout<AudioBuffer>.stride else { return -1 }
        return UnsafeMutableAudioBufferListPointer(list).reduce(0) { $0 + Int($1.mNumberChannels) }
    }
    static func provenance(_ id: AudioObjectID, uid: String, hidden: Bool, read: PropertyRead = systemRead) -> Bool {
        guard definitions.contains(where: { $0.uid == uid && $0.hidden == hidden }),
              id != kAudioObjectUnknown,
              translate(AudioObjectID(kAudioObjectSystemObject), kAudioHardwarePropertyTranslateUIDToDevice, uid, read: read) == id,
              number(id, kAudioObjectPropertyClass, read: read) == kAudioDeviceClassID,
              string(id, kAudioDevicePropertyDeviceUID, read: read) == uid,
              number(id, kAudioDevicePropertyTransportType, read: read) == kAudioDeviceTransportTypeVirtual,
              number(id, kAudioDevicePropertyIsHidden, read: read) == (hidden ? 1 : 0),
              let plugin = translate(AudioObjectID(kAudioObjectSystemObject), kAudioHardwarePropertyTranslateBundleIDToPlugIn, bundleID, read: read),
              number(plugin, kAudioObjectPropertyClass, read: read) == kAudioPlugInClassID,
              string(plugin, kAudioPlugInPropertyBundleID, read: read) == bundleID,
              translate(plugin, kAudioPlugInPropertyTranslateUIDToDevice, uid, read: read) == id else { return false }
        // Public HAL normalizes device Owner to the system object. Its registered
        // plugin's exact UID mapping establishes provenance, even for hidden
        // endpoints omitted from both system and plugin DeviceList properties.
        return true
    }
    static func streams(_ id: AudioObjectID, _ scope: AudioObjectPropertyScope, readSize: PropertySize = systemSize) -> Int {
        var address = AudioObjectPropertyAddress(mSelector: kAudioDevicePropertyStreams, mScope: scope, mElement: kAudioObjectPropertyElementMain)
        var bytes: UInt32 = 0
        guard readSize(id, &address, 0, nil, &bytes) == noErr, bytes <= 65536,
              bytes % UInt32(MemoryLayout<AudioStreamID>.size) == 0 else { return -1 }
        return Int(bytes) / MemoryLayout<AudioStreamID>.size
    }
    static func matchesDirection(_ id: AudioObjectID, _ direction: AudioEndpointDirection) -> Bool {
        direction.accepts(inputChannels: channels(id, kAudioObjectPropertyScopeInput),
                          outputChannels: channels(id, kAudioObjectPropertyScopeOutput),
                          inputStreams: streams(id, kAudioObjectPropertyScopeInput),
                          outputStreams: streams(id, kAudioObjectPropertyScopeOutput))
    }
    static func ensure() throws -> [String] {
        for definition in definitions {
            guard let id = find(definition.uid) else { throw Failure.directionalDeviceRequired }
            guard string(id, kAudioDevicePropertyDeviceUID) == definition.uid,
                  string(id, kAudioObjectPropertyName) == definition.name,
                  provenance(id, uid: definition.uid, hidden: definition.hidden) else { throw Failure.wrongTopology }
            guard matchesDirection(id, definition.direction) else { throw Failure.directionalDeviceRequired }
        }
        guard let visible = devices() else { throw Failure.wrongTopology }
        var visibleUIDs: [String] = []
        for id in visible {
            guard let uid = string(id, kAudioDevicePropertyDeviceUID) else { throw Failure.wrongTopology }
            visibleUIDs.append(uid)
        }
        guard DirectionalAudioCatalog.accepts(visibleUIDs) else { throw Failure.legacyAliasesStillVisible }
        // Inspection only: no creation/removal, IO, route, rate or default writes.
        return definitions.filter { !$0.hidden }.map { $0.name }
    }
}
