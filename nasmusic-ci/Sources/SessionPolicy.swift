import Foundation

// Playback generations are independent of track revisions: a late response from
// a retired account must never revoke a new playback session.
struct SessionPolicy {
    private(set) var generation = 0
    mutating func retire() { generation += 1 }
    func rejects(status: Int, generation captured: Int, cancelled: Bool) -> Bool {
        !cancelled && captured == generation && (status == 401 || status == 403)
    }
    static func artworkURL(base: URL, id: String?) -> URL? {
        guard let id, id.range(of: "^(?:[0-9]{1,20}|mp3-[0-9]{1,20}|qq-[A-Za-z0-9]{1,32}|kuwo-[0-9]{1,20})$", options: .regularExpression) != nil,
              var parts = URLComponents(url: base, resolvingAgainstBaseURL: false) else { return nil }
        parts.path = "/api/music/artwork"
        parts.queryItems = [URLQueryItem(name: "track", value: id), URLQueryItem(name: "size", value: "256")]
        return parts.url
    }
}
