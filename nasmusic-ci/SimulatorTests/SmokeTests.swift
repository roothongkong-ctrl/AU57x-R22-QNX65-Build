import XCTest
import WebKit
import AVFoundation

// Runs on an actual iOS simulator, without contacting the production NAS.
final class SmokeTests: XCTestCase {
    @MainActor func testBridgeLoadsInWebKit() async throws {
        let source = try XCTUnwrap(Bundle(for: Self.self).url(forResource: "native-music", withExtension: "js"))
        let adapter = try String(contentsOf: source, encoding: .utf8)
        let config = WKWebViewConfiguration()
        config.userContentController.addUserScript(WKUserScript(source: "window.NASNativeMusic={version:2,send:m=>window.messages.push(m)};window.messages=[];\n" + adapter, injectionTime: .atDocumentStart, forMainFrameOnly: true))
        let web = WKWebView(frame: .zero, configuration: config)
        web.loadHTMLString("<html><body><audio id='player'></audio></body></html>", baseURL: URL(string: "https://example.invalid/music/"))
        for _ in 0..<100 {
            if let ready = try? await web.evaluateJavaScript("document.readyState === 'complete' && typeof window.NASNativeAudio === 'function'"), (ready as? Bool) == true { break }
            try await Task.sleep(nanoseconds: 100_000_000)
        }
        let result = try await web.evaluateJavaScript("""
        (() => {
          const a=document.getElementById('player');
          const b=window.NASNativeAudio(a,src=>({tracks:[],start:0,position:0}),()=>{});
          a.src='/api/music/play-file?id=test';
          if(!b.active())return 'not-active';
          a.currentTime=2;
          a.play().catch(()=>{});
          const m=window.messages.at(-1);
          if(m.action!=='start'||m.position!==2)return 'bad-start';
          window.NASNativeAudioState({serial:m.serial,event:'playing',ready:true,paused:false,time:2});
          if(a.paused||a.currentTime!==2)return 'bad-state';
          a.pause();
          if(window.messages.at(-1).action!=='pause')return 'bad-pause';
          a.removeAttribute('src');
          if(b.active()||window.messages.at(-1).action!=='stop')return 'bad-stop';
          return 'ok';
        })()
        """)
        XCTAssertEqual(result as? String, "ok")
    }

    @MainActor func testSystemQueueAdvancesLocalAudio() async throws {
        // Synthetic silent PCM audio, not any copyrighted music or network source.
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: dir) }
        let format = try XCTUnwrap(AVAudioFormat(standardFormatWithSampleRate: 16000, channels: 1))
        let buffer = try XCTUnwrap(AVAudioPCMBuffer(pcmFormat: format, frameCapacity: 16000))
        buffer.frameLength = 16000
        let samples = try XCTUnwrap(buffer.floatChannelData)[0]
        for i in 0..<Int(buffer.frameLength) { samples[i] = 0 }
        func make(_ name: String) throws -> AVPlayerItem {
            let url = dir.appendingPathComponent(name + ".caf")
            let file = try AVAudioFile(forWriting: url, settings: format.settings)
            try file.write(from: buffer)
            return AVPlayerItem(url: url)
        }
        let first = try make("one"), second = try make("two")
        let player = AVQueuePlayer(items: [first, second])
        player.actionAtItemEnd = .advance
        defer { player.pause(); player.removeAllItems() }
        try AVAudioSession.sharedInstance().setCategory(.playback)
        try AVAudioSession.sharedInstance().setActive(true)
        defer { try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation) }
        player.play()
        var advanced = false
        for _ in 0..<200 {
            if player.currentItem === second { advanced = true; break }
            if first.status == .failed { XCTFail(first.error?.localizedDescription ?? "first item failed"); break }
            try await Task.sleep(nanoseconds: 50_000_000)
        }
        XCTAssertTrue(advanced, "AVQueuePlayer did not advance to its second item")
    }
}
