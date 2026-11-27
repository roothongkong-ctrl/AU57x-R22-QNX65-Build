import Foundation

// Per queue slot, bounded even when AVPlayer sends multiple failure notifications.
struct SourceRecovery {
    enum Action: Equatable { case refresh, mp3, stop }
    private var attempts: [Int: Int] = [:]
    mutating func next(slot: Int, canResolve: Bool, lossless: Bool, library: Bool) -> Action {
        guard canResolve else { return .stop }
        let count = attempts[slot, default: 0]
        guard count < 2 else { return .stop }
        attempts[slot] = count + 1
        if count == 0 { return .refresh }
        return lossless && !library ? .mp3 : .stop
    }
    mutating func reset() { attempts.removeAll() }
    static func retryable(_ error: Error) -> Bool {
        guard let e = error as? URLError else { return false }
        return [.timedOut,.networkConnectionLost,.notConnectedToInternet,.cannotConnectToHost,.cannotFindHost,.dnsLookupFailed].contains(e.code)
    }
}
