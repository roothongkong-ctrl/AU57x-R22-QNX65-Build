import Foundation

// Pure payload construction; never records a track without an authenticated NAS reference.
struct PlaybackRecord {
    static func payload(library: String?, playback: String?, position: Double, updated: Int64, queue: [String:Any]?, favorites: [String]?) -> [String:Any]? {
        guard position.isFinite, position >= 0, updated > 0 else { return nil }
        var result: [String:Any] = ["position": Int(min(position,86400)), "updated": updated, "ended":false, "queue":queue.map { $0 as Any } ?? NSNull()]
        if let library, !library.isEmpty { result["library"] = library }
        else if let playback, playback.range(of:"^[a-f0-9]{32}$",options:.regularExpression) != nil { result["playback"] = playback }
        else { return nil }
        if let favorites, !favorites.isEmpty { result["favorites"] = favorites }
        return result
    }
}
