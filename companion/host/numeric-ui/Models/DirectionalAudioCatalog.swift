import Foundation

/// Pure validation of a successful visible-device metadata snapshot.
enum DirectionalAudioCatalog {
    static let publicUIDs = Set(["local.mrkillbob.StackChan.hal.microphone.v2", "local.mrkillbob.StackChan.hal.speaker.v2"])
    static let forbiddenVisibleUIDs = Set([
        "local.mrkillbob.StackChan.microphone.v1", "local.mrkillbob.StackChan.speaker.v1",
        "local.mrkillbob.StackChan.hal.feed.v2", "local.mrkillbob.StackChan.hal.return.v2"
    ])
    static func accepts(_ visibleUIDs: [String]?) -> Bool {
        guard let visibleUIDs else { return false }
        let found = Set(visibleUIDs)
        return publicUIDs.isSubset(of: found) && found.isDisjoint(with: forbiddenVisibleUIDs)
    }
}
