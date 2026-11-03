import SwiftUI
import AppKit
final class AppDelegate: NSObject, NSApplicationDelegate {
    weak var controller: PairingController?
    weak var lifecycle: CompanionLifecycle?
    func applicationDidFinishLaunching(_ notification: Notification) { NSApp.setActivationPolicy(.regular); NSApp.activate(ignoringOtherApps: true) }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        // Lifecycle close mutes/disarms and releases first (it drives controller.shutdown);
        // a bare controller still shuts down if no lifecycle exists. Window close and Quit are
        // the same event here, so both are reported as app_quit.
        if let lifecycle = lifecycle {
            lifecycle.close(reason: .appQuit) { closed in DispatchQueue.main.async { sender.reply(toApplicationShouldTerminate: closed) } }
            return .terminateLater
        }
        guard let controller = controller else { return .terminateNow }
        controller.shutdown { closed in DispatchQueue.main.async { sender.reply(toApplicationShouldTerminate: closed) } }
        return .terminateLater
    }
}
@main struct StackChanPairingApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var delegate
    @StateObject private var controller = PairingController(
        approved: CommandLine.arguments.contains("--approved-test"),
        root: URL(fileURLWithPath: ProcessInfo.processInfo.environment["STACKCHAN_ROOT"] ?? (Bundle.main.object(forInfoDictionaryKey: "StackChanRoot") as? String) ?? "/nonexistent/stackchan-unconfigured"),
        audioApproved: CommandLine.arguments.contains("--approved-audio"),
        audioMode: CommandLine.arguments.contains("--native-conversation") ? .nativeConversation : .transportTest)
    private static let projectRoot = URL(fileURLWithPath: ProcessInfo.processInfo.environment["STACKCHAN_ROOT"] ?? (Bundle.main.object(forInfoDictionaryKey: "StackChanRoot") as? String) ?? "/nonexistent/stackchan-unconfigured")
    private static let robotLink = PairingControllerRobotLink()
    // Read-only view of the persistent host registry (state/counts only). Path A: the user runs
    // the Mira call themselves, so the route is manual and never claims "In call".
    @StateObject private var lifecycle = CompanionLifecycle(
        trust: PersistentRegistryTrust(reader: ConfiguredRegistryReader(projectRoot: Self.projectRoot, bindingURL: ProcessInfo.processInfo.environment["STACKCHAN_BINDING_CONFIG"].map { URL(fileURLWithPath: $0) })),
        route: ManualMacCallRoute(), link: robotLink, resources: WorkerAudioResources.shared)
    var body: some Scene {
        Window("StackChan Companion", id: "pairing") {
            ContentView(controller: controller, lifecycle: lifecycle, localStop: { Self.robotLink.noteLocalStop() }).onAppear {
                delegate.controller = controller; delegate.lifecycle = lifecycle
                WorkerAudioResources.shared.bind(controller)
                Self.robotLink.bind(controller)
                Self.robotLink.onConsentChanged = { [weak lifecycle = lifecycle] epoch, consented in
                    guard let lifecycle = lifecycle else { return }
                    if consented { lifecycle.robotConsentReported(epoch: epoch) }
                    else { lifecycle.robotConsentRevoked(epoch: epoch) }
                }
            }
        }.windowResizability(.contentSize)
    }
}
