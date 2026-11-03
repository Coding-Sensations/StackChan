import Foundation

enum AudioRouteCatalog {
    static func requiredEndpointsAvailable() -> Bool {
        // Read-only: prove the public directions, hidden endpoints, plugin identity
        // and complete visible catalog using the same verifier as setup.
        (try? StackChanAudioDevices.ensure()) != nil
    }
}
