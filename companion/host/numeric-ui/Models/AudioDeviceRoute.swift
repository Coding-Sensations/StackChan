import Foundation

struct AudioDeviceRoute {
    static let voiceInputUID = "local.mrkillbob.StackChan.hal.microphone.v2"
    static let voiceOutputUID = "local.mrkillbob.StackChan.hal.speaker.v2"

    let endpointsAvailable: Bool

    var canUseStackChan: Bool { endpointsAvailable }

    func workerPayload() -> [String: Any]? {
        guard canUseStackChan else { return nil }
        return [
            "command": "connect",
            "audio_route": [
                "input_uid": Self.voiceInputUID,
                "output_uid": Self.voiceOutputUID
            ]
        ]
    }
}
