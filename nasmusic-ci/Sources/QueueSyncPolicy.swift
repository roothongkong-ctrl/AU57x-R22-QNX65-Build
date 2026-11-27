import Foundation

// A replacement is allowed only for the exact currently acknowledged audio item.
enum QueueSyncPolicy {
    static func index(slots: [Int], start: Int, currentURL: URL?, incomingURL: URL?, expectedSlot: Int, currentSlot: Int, busy: Bool) -> Int? {
        guard !busy, expectedSlot == currentSlot, let currentURL, currentURL == incomingURL,
              !slots.isEmpty, slots.count <= 400, slots.allSatisfy({$0 >= 0}), Set(slots).count == slots.count else { return nil }
        return slots.firstIndex(of: start)
    }
}
