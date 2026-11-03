import Foundation

/// Public endpoint direction is distinct from the internal loopback bus direction.
enum AudioEndpointDirection {
    case microphone, speaker
    func accepts(inputChannels: Int, outputChannels: Int, inputStreams: Int, outputStreams: Int) -> Bool {
        switch self {
        case .microphone: return inputChannels == 2 && outputChannels == 0 && inputStreams > 0 && outputStreams == 0
        case .speaker: return inputChannels == 0 && outputChannels == 2 && inputStreams == 0 && outputStreams > 0
        }
    }
}
