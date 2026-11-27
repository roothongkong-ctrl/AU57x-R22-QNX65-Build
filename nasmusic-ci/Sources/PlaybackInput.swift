import Foundation

// Validate messages before they can mutate the native queue.
enum PlaybackInput {
    static func valid(slots: [Int], position: Double, sleepUntil: Double) -> Bool {
        !slots.isEmpty && slots.count <= 400 && slots.allSatisfy { $0 >= 0 } && Set(slots).count == slots.count && seek(position) != nil && validDeadline(sleepUntil)
    }
    static func validDeadline(_ value: Double) -> Bool {
        // Bound Date/DispatchTime conversions against malformed page payloads.
        value.isFinite && value >= 0 && value <= 4_102_444_800_000
    }
    static func seek(_ seconds: Double) -> Double? {
        guard seconds.isFinite, seconds >= 0, seconds <= 86_400 else { return nil }
        return seconds
    }
}
