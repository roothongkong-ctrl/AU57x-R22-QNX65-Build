import Foundation
import AVFoundation
import MediaPlayer
import UIKit

@MainActor final class NativeQueue: NSObject {
    static let base = URL(string:"https://example.invalid:1232")!
    static func allowed(_ u: URL) -> Bool { u.scheme == "https" && u.host == base.host && u.port == base.port && u.user == nil && u.password == nil && ["/api/music/play-file","/api/music/private-audio"].contains(u.path) }
    struct Track: Codable {
        var slot: Int; var title: String; var artist: String; var duration: Double?
        var url: URL?; var playback: String?; var library: String?; var rowId: String?; var kind: String?; var favorite: String?; var search: String?; var row: ChartRow?; var mix: Bool?; var format: String?
    }
    struct ChartRow: Codable { var title: String; var artist: String; var duration: Double?; var nativeId: String?; var cover: String? }
    struct Plan: Decodable { var tracks: [Track]; var start: Int; var position: Double; var mode: String; var single: Bool; var sleepMinutes: Int; var sleepUntil: Double; var quality: String; var saveHistory: Bool?; var queuePlatform: String? }
    private let player = AVQueuePlayer()
    private var activeItem: AVPlayerItem?, queuedItem: AVPlayerItem?, currentObservation: NSKeyValueObservation?
    private var failedSlots = Set<Int>(), resolving = false, hasStarted = false
    private var sourceRecovery = SourceRecovery()
    private var sessionPolicy = SessionPolicy()
    private var artworkTask: Task<Void,Never>?, artworkURL: URL?, artwork: MPMediaItemArtwork?
    private var interruptionObserver: NSObjectProtocol?, routeObserver: NSObjectProtocol?
    private var interruptedWhilePlaying = false
    private var plan: Plan?, index = 0, serial = 0, revision = 0
    private var cookies: [HTTPCookie] = [], task: Task<Void,Never>?, preload: Task<Void,Never>?, prepared: Track?, preparedIndex: Int?, nextIndex: Int?, randomBag:[Int]=[], history:[Int]=[]
    private var endObserver: NSObjectProtocol?, failedObserver: NSObjectProtocol?, ticker: Any?, status: NSKeyValueObservation?, control: NSKeyValueObservation?
    private var commandTargets: [(MPRemoteCommand,Any)] = []
    private var recent: [Track] = []
    private var background = false, lastSavedAt: Int64 = 0, lastSavedSlot: Int?, recordTask: Task<Void,Never>?
    private var wantsPlay = false, reportedPlaying = false, sleeping = false, sleepDeadline: DispatchWorkItem?, pendingSeek: Double = 0
    var emit: (([String:Any])->Void)?
    override init() {
        super.init()
        player.actionAtItemEnd = .advance
        currentObservation=player.observe(\.currentItem,options:[.new]){[weak self] _,_ in DispatchQueue.main.async{self?.adoptCurrentItem()}}
        endObserver = NotificationCenter.default.addObserver(forName:.AVPlayerItemDidPlayToEndTime,object:nil,queue:.main) { [weak self] n in
            DispatchQueue.main.async { guard let self, let item=n.object as? AVPlayerItem else{return};self.ended(item) }
        }
        failedObserver = NotificationCenter.default.addObserver(forName:.AVPlayerItemFailedToPlayToEndTime,object:nil,queue:.main) { [weak self] n in
            DispatchQueue.main.async { guard let self, let item=n.object as? AVPlayerItem, item === self.activeItem else{return};self.adoptCurrentItem();guard item === self.activeItem else{return};self.recover("音频播放中断") }
        }
        ticker=player.addPeriodicTimeObserver(forInterval:CMTime(seconds:0.5,preferredTimescale:600),queue:.main){[weak self] _ in DispatchQueue.main.async{self?.adoptCurrentItem();self?.publish("timeupdate")}}
        control=player.observe(\.timeControlStatus,options:[.new]){[weak self] _,_ in DispatchQueue.main.async{self?.controlChanged()}}
        let c=MPRemoteCommandCenter.shared()
        func register(_ command: MPRemoteCommand,_ handler:@escaping (NativeQueue,MPRemoteCommandEvent)->Void){let t=command.addTarget{[weak self] e in guard let self else{return .commandFailed};DispatchQueue.main.async{handler(self,e)};return .success};commandTargets.append((command,t))}
        register(c.playCommand){q,_ in q.resume()};register(c.pauseCommand){q,_ in q.pause()};register(c.nextTrackCommand){q,_ in q.advance(1)};register(c.previousTrackCommand){q,_ in q.advance(-1)}
        register(c.togglePlayPauseCommand){q,_ in if q.wantsPlay{q.pause()}else{q.resume()}}
        interruptionObserver=NotificationCenter.default.addObserver(forName:AVAudioSession.interruptionNotification,object:nil,queue:.main){[weak self] n in
            let type=n.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt
            let options=n.userInfo?[AVAudioSessionInterruptionOptionKey] as? UInt ?? 0
            DispatchQueue.main.async{self?.interruption(type,options:options)}
        }
        routeObserver=NotificationCenter.default.addObserver(forName:AVAudioSession.routeChangeNotification,object:nil,queue:.main){[weak self] n in
            let reason=n.userInfo?[AVAudioSessionRouteChangeReasonKey] as? UInt
            DispatchQueue.main.async{if reason==AVAudioSession.RouteChangeReason.oldDeviceUnavailable.rawValue{self?.pause()}}
        }
        register(c.changePlaybackPositionCommand){q,e in if let e=e as? MPChangePlaybackPositionCommandEvent{q.seek(e.positionTime)}}
    }
    deinit { if let x=interruptionObserver{NotificationCenter.default.removeObserver(x)};if let x=routeObserver{NotificationCenter.default.removeObserver(x)};task?.cancel();preload?.cancel();artworkTask?.cancel();recordTask?.cancel();sleepDeadline?.cancel();if let x=endObserver{NotificationCenter.default.removeObserver(x)};if let x=failedObserver{NotificationCenter.default.removeObserver(x)};if let x=ticker{player.removeTimeObserver(x)};for (c,t) in commandTargets{c.removeTarget(t)} }
    func start(_ p:Plan, serial:Int, cookies:[HTTPCookie]) throws {
        guard PlaybackInput.valid(slots:p.tracks.map(\.slot),position:p.position,sleepUntil:p.sleepUntil),let i=p.tracks.firstIndex(where:{$0.slot==p.start}),let u=p.tracks[i].url,Self.allowed(u),p.tracks.allSatisfy({$0.url.map(Self.allowed) ?? true}) else{throw URLError(.badURL)}
        stop();try AVAudioSession.sharedInstance().setCategory(.playback,mode:.default);try AVAudioSession.sharedInstance().setActive(true)
        self.serial=serial;plan=p;index=i;self.cookies=cookies.filter{let d=$0.domain.trimmingCharacters(in:CharacterSet(charactersIn:"."));let h=Self.base.host!;return h==d||h.hasSuffix("."+d)}
        wantsPlay=true;pendingSeek=max(0,p.position);scheduleSleep();load(p.tracks[i],announce:false);prepareNext()
    }
    func stop(){retire(save:true)}
    private func retire(save:Bool){if save{saveRecord(force:true)}else{recordTask?.cancel()};sessionPolicy.retire();artworkTask?.cancel();artworkTask=nil;artworkURL=nil;artwork=nil;revision+=1;task?.cancel();preload?.cancel();sleepDeadline?.cancel();task=nil;preload=nil;prepared=nil;preparedIndex=nil;nextIndex=nil;randomBag=[];history=[];wantsPlay=false;reportedPlaying=false;status=nil;activeItem=nil;queuedItem=nil;failedSlots=[];sourceRecovery.reset();resolving=false;hasStarted=false;interruptedWhilePlaying=false;pendingSeek=0;player.pause();player.removeAllItems();plan=nil;recent=[];cookies=[];lastSavedAt=0;lastSavedSlot=nil;MPNowPlayingInfoCenter.default().nowPlayingInfo=nil}
    func replaceQueue(_ incoming:Plan, expectedSlot:Int) {
        adoptCurrentItem()
        guard PlaybackInput.valid(slots:incoming.tracks.map(\.slot),position:incoming.position,sleepUntil:incoming.sleepUntil),let p=plan, let proposed=incoming.tracks.first(where:{$0.slot==incoming.start}),
            incoming.tracks.allSatisfy({$0.url.map(Self.allowed) ?? true}),
            let n=QueueSyncPolicy.index(slots:incoming.tracks.map(\.slot),start:incoming.start,currentURL:p.tracks[index].url,incomingURL:proposed.url,expectedSlot:expectedSlot,currentSlot:p.tracks[index].slot,busy:resolving || activeItem == nil || activeItem !== player.currentItem) else { return }
        // Retain the already playing item and resolved reference; only replace future recipes.
        var replacement=incoming;var current=p.tracks[index];current.slot=proposed.slot;replacement.tracks[n]=current
        clearPreload();plan=replacement;index=n;randomBag=[];history=[];failedSlots=[];sourceRecovery.reset();scheduleSleep();prepareNext()
        publish("queue-updated",track:current)
    }
    func policy(mode:String,minutes:Int,until:Double){adoptCurrentItem();guard PlaybackInput.validDeadline(until),var p=plan else{return};if p.mode==mode && p.sleepMinutes==minutes && p.sleepUntil==until{return};if p.mode != mode{randomBag=[];history=[]};p.mode=mode;p.sleepMinutes=minutes;p.sleepUntil=until;plan=p;scheduleSleep();prepareNext()}
    private func scheduleSleep(){sleepDeadline?.cancel();sleeping=false;guard let p=plan,p.sleepUntil>0 else{return};let w=DispatchWorkItem{[weak self] in self?.sleeping=true;self?.clearPreload();self?.pause()};sleepDeadline=w;DispatchQueue.main.asyncAfter(deadline:.now()+max(0,p.sleepUntil/1000-Date().timeIntervalSince1970),execute:w)}
    private func makeItem(_ track:Track)->AVPlayerItem?{
        guard let u=track.url,Self.allowed(u) else{return nil}
        let item=AVPlayerItem(asset:AVURLAsset(url:u,options:[AVURLAssetHTTPCookiesKey:cookies]));item.preferredForwardBufferDuration=12;return item
    }
    private func load(_ track:Track,announce:Bool){
        guard let item=makeItem(track) else{recover("音频地址无效");return}
        plan?.tracks[index]=track;resolving=false;reportedPlaying=false;status=nil;activeItem=nil;queuedItem=nil;player.removeAllItems();activeItem=item;player.insert(item,after:nil)
        observeActive(item);if announce{publish("track",track:track)};if wantsPlay{player.play()}
    }
    private func observeActive(_ item:AVPlayerItem){
        status=item.observe(\.status,options:[.initial,.new]){[weak self,weak item] _,_ in DispatchQueue.main.async{
            guard let self,let item,item===self.activeItem,item===self.player.currentItem else{return}
            if item.status == .failed{self.recover("音频无法解码或连接失败")}
            else if item.status == .readyToPlay{if self.pendingSeek>0{let t=self.pendingSeek;self.pendingSeek=0;self.seek(t)};self.publish("timeupdate");if self.wantsPlay{self.player.play()}}
        }}
    }
    private func adoptCurrentItem(){
        guard let item=player.currentItem,item === queuedItem,let n=preparedIndex,let track=prepared,plan != nil else{return}
        if plan?.mode=="random" && n != index{history.append(index);randomBag.removeAll{$0==n}}
        revision+=1;task?.cancel();preload?.cancel();index=n;plan?.tracks[n]=track;activeItem=item;queuedItem=nil;prepared=nil;preparedIndex=nil;nextIndex=nil;pendingSeek=0;reportedPlaying=false;resolving=false
        observeActive(item);publish("track",track:track);controlChanged();prepareNext()
    }
    private func ended(_ item:AVPlayerItem){
        adoptCurrentItem();guard item === activeItem,!resolving else{return}
        // AVQueuePlayer advances itself if a second item was inserted. Never advance twice.
        if queuedItem != nil && player.currentItem === item{return}
        activeItem=nil;status=nil
        guard wantsPlay,!sleeping else{return};advance(1,automatic:true)
    }
    private func controlChanged(){adoptCurrentItem();guard plan != nil,let item=activeItem,item === player.currentItem else{return};if player.timeControlStatus == .playing {if !reportedPlaying{hasStarted=true;failedSlots=[];reportedPlaying=true;if background, let t=plan?.tracks[index],recent.last?.slot != t.slot || recent.last?.url != t.url { recent.append(t);if recent.count>100{recent.removeFirst(recent.count-100)} };publish("playing")}}else if wantsPlay && player.timeControlStatus == .waitingToPlayAtSpecifiedRate{reportedPlaying=false;publish("waiting")}}
    private func interruption(_ type:UInt?,options:UInt){
        if type==AVAudioSession.InterruptionType.began.rawValue{let wasPlaying=wantsPlay;pause();interruptedWhilePlaying=wasPlaying}
        else if type==AVAudioSession.InterruptionType.ended.rawValue{let should=interruptedWhilePlaying;interruptedWhilePlaying=false;if should && AVAudioSession.InterruptionOptions(rawValue:options).contains(.shouldResume){resume()}}
    }
    private func candidate(_ direction:Int,automatic:Bool)->Int?{guard let p=plan,!p.tracks.isEmpty,!sleeping else{return nil};if automatic && p.sleepMinutes == -1{return nil};if automatic && p.mode=="single" && p.sleepMinutes != -2{return index};if p.single{return nil};if automatic && p.sleepMinutes == -2{return index+1<p.tracks.count ? index+1:nil};if p.mode=="random"{if direction<0{return history.last};if randomBag.isEmpty{randomBag=Array(p.tracks.indices.filter{$0 != index}).shuffled()};return randomBag.first ?? index};return (index+direction+p.tracks.count)%p.tracks.count}
    private func clearPreload(){
        preload?.cancel();preload=nil
        if let item=queuedItem,item !== player.currentItem{player.remove(item)}
        queuedItem=nil;prepared=nil;preparedIndex=nil;nextIndex=nil
    }
    private func prepareNext(){
        clearPreload();nextIndex=candidate(1,automatic:true);guard let n=nextIndex,let p=plan else{return};let r=revision;let t=p.tracks[n]
        preload=Task{[weak self] in guard let self else{return};do{
            let resolved=try await self.resolve(t,quality:p.quality);try Task.checkCancellation();guard self.revision==r,self.nextIndex==n,let current=self.activeItem,current===self.player.currentItem,let item=self.makeItem(resolved) else{return}
            self.prepared=resolved;self.preparedIndex=n
            if self.player.canInsert(item,after:current){self.queuedItem=item;self.player.insert(item,after:current)}
        }catch{}}
    }
    func advance(_ direction:Int,automatic:Bool=false){
        adoptCurrentItem();guard plan != nil,!sleeping else{pause();return}
        guard let n=(automatic ? nextIndex ?? candidate(direction,automatic:true):candidate(direction,automatic:false)) else{pause();return}
        if !automatic{failedSlots=[];sourceRecovery.reset()};move(to:n,direction:direction,automatic:automatic)
    }
    private func move(to n:Int,direction:Int,automatic:Bool){
        guard let p=plan,p.tracks.indices.contains(n) else{return}
        let ready=preparedIndex==n ? prepared:nil
        if let item=queuedItem,preparedIndex==n,player.items().contains(where:{$0===item}),activeItem===player.currentItem{
            wantsPlay=true;player.advanceToNextItem();adoptCurrentItem();if wantsPlay{player.play()};return
        }
        revision+=1;let r=revision;task?.cancel();clearPreload()
        if p.mode=="random" && n != index{if direction>0{history.append(index);randomBag.removeAll{$0==n}}else{_ = history.popLast()}}
        index=n;wantsPlay=true;reportedPlaying=false;pendingSeek=0;status=nil;activeItem=nil;resolving=true;player.pause();player.removeAllItems();publish("waiting")
        task=Task{[weak self] in guard let self else{return};do{let t:Track;if let ready{t=ready}else{t=try await self.resolve(p.tracks[n],quality:p.quality)};try Task.checkCancellation();guard self.revision==r else{return};self.load(t,announce:true);self.prepareNext()}catch{if self.revision==r && !Task.isCancelled{self.resolving=false;self.recover("下一首音源暂未就绪")}}}
    }
    private func recover(_ message:String){
        guard let p=plan,wantsPlay,!sleeping,!resolving else{if !resolving{fail(message)};return}
        let t=p.tracks[index], library=t.kind == "library" || !(t.library ?? "").isEmpty
        let lossless=t.url?.path == "/api/music/private-audio" || t.format?.uppercased()=="FLAC" || p.quality=="lossless"
        let action=sourceRecovery.next(slot:index,canResolve:t.kind != nil || !(t.playback ?? "").isEmpty || library,lossless:lossless,library:library)
        if action != .stop {
            let time=player.currentTime().seconds
            if time.isFinite && time>0{pendingSeek=time}
            revision+=1;let r=revision;task?.cancel();clearPreload();status=nil;activeItem=nil;resolving=true;reportedPlaying=false;player.pause();player.removeAllItems();publish("waiting")
            var fresh=t;fresh.url=nil
            let quality=action == .mp3 ? "mp3" : p.quality
            task=Task{[weak self] in guard let self else{return};do{
                try await Task.sleep(nanoseconds:750_000_000)
                let resolved=try await self.resolve(fresh,quality:quality)
                try Task.checkCancellation();guard self.revision==r else{return};self.load(resolved,announce:true);self.prepareNext()
            }catch{if self.revision==r && !Task.isCancelled{self.resolving=false;self.recover(message)}}}
            return
        }
        guard hasStarted else{fail(message);return}
        failedSlots.insert(index);let options=RecoveryPolicy.candidates(count:p.tracks.count,index:index,failed:failedSlots,mode:p.mode,single:p.single,sleepMinutes:p.sleepMinutes)
        guard let next=options.first else{fail(message);return};move(to:next,direction:1,automatic:true)
    }
    func pause(){saveRecord(force:true);interruptedWhilePlaying=false;wantsPlay=false;reportedPlaying=false;player.pause();publish("pause")}
    func resume(){guard plan != nil else{return};sleeping=false;wantsPlay=true;try? AVAudioSession.sharedInstance().setActive(true);if (player.currentItem==nil || player.currentItem?.status == .failed) && !resolving{failedSlots=[];sourceRecovery.reset();if let t=plan?.tracks[index],t.kind != nil || !(t.playback ?? "").isEmpty || !(t.library ?? "").isEmpty{plan?.tracks[index].url=nil};move(to:index,direction:1,automatic:false)}else if !resolving{player.play();controlChanged()}}
    func seek(_ seconds:Double){
        guard let target=PlaybackInput.seek(seconds),plan != nil else{return}
        guard let item=activeItem,item === player.currentItem,item.status == .readyToPlay else{pendingSeek=target;return}
        let r=revision;player.seek(to:CMTime(seconds:target,preferredTimescale:600)){[weak self,weak item] finished in DispatchQueue.main.async{
            guard let self,let item,finished,self.revision==r,item === self.activeItem,item === self.player.currentItem else{return};self.publish("seeked")
        }}
    }
    private func fail(_ message:String){revision+=1;task?.cancel();clearPreload();resolving=false;wantsPlay=false;reportedPlaying=false;player.pause();publish("error",error:message)}
    func refresh(){guard let p=plan,p.tracks.indices.contains(index),p.tracks[index].url != nil else{return};
        if !background,!recent.isEmpty,let data=try? JSONEncoder().encode(recent),let rows=try? JSONSerialization.jsonObject(with:data){emit?(["serial":serial,"event":"recent","rows":rows]);recent=[]}
        publish(player.timeControlStatus == .playing ? "playing" : wantsPlay ? "waiting" : "pause",track:p.tracks[index])}
    private func publish(_ event:String,track:Track?=nil,error:String?=nil){if event == "playing" || event == "timeupdate" {saveRecord(force:false)};guard let p=plan,p.tracks.indices.contains(index) else{return};let t=p.tracks[index], time=player.currentTime().seconds,d=player.currentItem?.duration.seconds ?? 0;let ready=player.currentItem?.status == .readyToPlay
        var payload:[String:Any]=["serial":serial,"event":event,"paused":!wantsPlay,"ready":ready,"time":time.isFinite ? time:0,"duration":d.isFinite && d>0 ? (d as Any):NSNull()]
        if let track,let data=try? JSONEncoder().encode(track),let json=try? JSONSerialization.jsonObject(with:data){payload["track"]=json};if let error{payload["error"]=error};emit?(payload)
        updateArtwork(t)
        var info:[String:Any]=[MPMediaItemPropertyTitle:t.title,MPMediaItemPropertyArtist:t.artist,MPNowPlayingInfoPropertyPlaybackRate:player.rate,MPNowPlayingInfoPropertyElapsedPlaybackTime:time.isFinite ? time:0];if d.isFinite && d>0{info[MPMediaItemPropertyPlaybackDuration]=d};if let artwork{info[MPMediaItemPropertyArtwork]=artwork};MPNowPlayingInfoCenter.default().nowPlayingInfo=info
    }
    private func updateArtwork(_ track:Track){
        let url=SessionPolicy.artworkURL(base:Self.base,id:track.row?.nativeId) ?? SessionPolicy.artworkURL(base:Self.base,id:track.rowId)
        guard url != artworkURL else{return}
        artworkTask?.cancel();artwork=nil;artworkURL=url
        guard let url else{return}
        let generation=sessionPolicy.generation
        let eligible=cookies.filter{c in let p=c.path;return (c.expiresDate.map{$0>Date()} ?? true) && (url.path==p || url.path.hasPrefix(p.hasSuffix("/") ? p:p+"/"))}
        artworkTask=Task{[weak self] in
            var req=URLRequest(url:url);req.timeoutInterval=8;req.setValue(HTTPCookie.requestHeaderFields(with:eligible)["Cookie"],forHTTPHeaderField:"Cookie")
            let config=URLSessionConfiguration.ephemeral;config.httpShouldSetCookies=false
            let session=URLSession(configuration:config,delegate:NoRedirect(),delegateQueue:nil);defer{session.finishTasksAndInvalidate()}
            do{let (data,response)=try await session.data(for:req);try Task.checkCancellation()
                guard let self,self.sessionPolicy.generation==generation,self.artworkURL==url,
                    let http=response as? HTTPURLResponse,http.statusCode==200,data.count<=2_000_000,
                    let image=UIImage(data:data),image.size.width<=2048,image.size.height<=2048 else{return}
                self.artwork=MPMediaItemArtwork(boundsSize:image.size){_ in image}
                self.refresh()
            }catch{ /* artwork is optional; do not interrupt audio */ }
        }
    }
    func setBackground(_ value:Bool){
        if !value {saveRecord(force:true)}
        background=value
        if value {saveRecord(force:true)}
    }
    private func saveRecord(force:Bool){
        guard background,let p=plan,p.saveHistory == true,p.tracks.indices.contains(index),let item=activeItem,item === player.currentItem,item.status == .readyToPlay,hasStarted else{return}
        let t=p.tracks[index], now=Int64(Date().timeIntervalSince1970*1000)
        guard force || lastSavedSlot != t.slot || now-lastSavedAt >= 10_000 else{return}
        var snapshot:[String:Any]?; var favorites:[String]?
        if p.single {snapshot=["type":"single"];if let f=t.favorite,!f.isEmpty{favorites=[f]}}
        else if let f=t.favorite,!f.isEmpty {let order=p.tracks.compactMap{$0.favorite}.filter{!$0.isEmpty};if order.contains(f){snapshot=["type":"favorites","order":order];favorites=order}}
        else if let platform=p.queuePlatform, !platform.isEmpty, p.tracks.allSatisfy({$0.row != nil}) {
            let rows=p.tracks.compactMap{$0.row}.compactMap{row -> [String:Any]? in guard let d=try? JSONEncoder().encode(row) else{return nil};return (try? JSONSerialization.jsonObject(with:d)) as? [String:Any]}
            if rows.count==p.tracks.count && rows.count <= (platform == "mix" ? 400:100){snapshot=["type":"chart","platform":platform,"index":index,"rows":rows]}
        }
        guard let payload=PlaybackRecord.payload(library:t.library,playback:t.playback,position:player.currentTime().seconds,updated:now,queue:snapshot,favorites:favorites) else{return}
        lastSavedAt=now;lastSavedSlot=t.slot
        let capturedCookies=cookies, previous=recordTask, generation=sessionPolicy.generation
        // Cancel obsolete pending saves to bound the backlog. The NAS rejects older timestamps.
        previous?.cancel()
        recordTask=Task{[weak self] in
            await previous?.value
            guard let self,!Task.isCancelled else{return}
            do{_ = try await self.post("last-play",payload,cookieSnapshot:capturedCookies,recordGeneration:generation)}catch{if !Task.isCancelled && self.lastSavedAt==now{self.lastSavedAt=0;self.lastSavedSlot=nil}}
        }
    }
    private func resolve(_ input:Track,quality:String) async throws -> Track {
        if let u=input.url,Self.allowed(u){return input};var t=input;var body:[String:Any]=[:]
        switch t.kind {
        case "library":body["library"]=t.library ?? ""
        case "search":body=["id":t.rowId ?? "","search":t.search ?? ""]
        case "favorite":let j=try await post("favorite-play",["id":t.favorite ?? ""]);guard let ref=j["playback"] as? String else{throw URLError(.cannotParseResponse)};body["playback"]=ref;t.playback=ref;applyItem(j["item"],to:&t)
        case "chart":guard let row=t.row,let bytes=try? JSONEncoder().encode(row),let json=try? JSONSerialization.jsonObject(with:bytes) else{throw URLError(.cannotParseResponse)};var match:[String:Any]?;for _ in 0..<5{let j=try await post("chart-ready",["rows":[json],"wait":true,"mix":t.mix ?? false]);if let first=(j["matches"] as? [[String:Any]])?.first{match=first;break};try await Task.sleep(nanoseconds:500_000_000)};guard let m=match,let ref=m["playback"] as? String else{throw URLError(.resourceUnavailable)};body["playback"]=ref;t.playback=ref;applyItem(m["item"],to:&t)
        default:if let l=t.library,!l.isEmpty{body["library"]=l}else if let ref=t.playback,!ref.isEmpty{body["playback"]=ref}else{throw URLError(.badURL)}
        }
        if quality=="mp3"||quality=="lossless"{body["quality"]=quality}
        for attempt in 0..<60{try Task.checkCancellation();body["retry"]=attempt==0;let j=try await pollPlay(body);if let ref=j["playback"] as? String{body["playback"]=ref;t.playback=ref};if let q=j["qualityPreference"] as? String{body["quality"]=q};let state=j["state"] as? String
            if state=="ready"||state=="streaming"{applyItem(j["item"],to:&t);var url=URLComponents(url:Self.base,resolvingAgainstBaseURL:false)!;let format=j["format"] as? String;t.format=format?.uppercased();if j["privateLossless"] as? Bool==true{url.path="/api/music/private-audio";url.queryItems=[.init(name:"quality",value:"lossless"),.init(name:"ref",value:t.playback ?? "")]}else{guard let id=j["id"] as? String else{throw URLError(.cannotParseResponse)};url.path="/api/music/play-file";var items=[URLQueryItem(name:"id",value:id)];if j["favorite"] as? Bool==true{items.append(.init(name:"favorite",value:"1"))}else if j["library"] as? Bool==true{items.append(.init(name:"library",value:"1"))};if format=="mp3" && j["library"] as? Bool != true{items.append(.init(name:"format",value:"mp3"))};url.queryItems=items};t.url=url.url;return t}
            if state=="failed"{throw URLError(.resourceUnavailable)};try await Task.sleep(nanoseconds:attempt<10 ? 350_000_000:900_000_000)
        };throw URLError(.timedOut)
    }
    private func pollPlay(_ body:[String:Any]) async throws -> [String:Any] {
        do { return try await post("play",body) }
        catch { guard SourceRecovery.retryable(error) else {throw error};try await Task.sleep(nanoseconds:1_000_000_000);try Task.checkCancellation();return try await post("play",body) }
    }
    private func applyItem(_ item:Any?,to t:inout Track){guard let row=item as? [String:Any] else{return};t.title=row["title"] as? String ?? t.title;t.artist=row["artist"] as? String ?? t.artist;t.rowId=row["id"] as? String ?? t.rowId}
    private func post(_ path:String,_ body:[String:Any], cookieSnapshot:[HTTPCookie]?=nil, recordGeneration:Int?=nil) async throws -> [String:Any]{try Task.checkCancellation();let generation=recordGeneration ?? sessionPolicy.generation;let url=Self.base.appendingPathComponent("api/music/"+path);var req=URLRequest(url:url);req.httpMethod="POST";req.timeoutInterval=30;req.setValue("application/json",forHTTPHeaderField:"Content-Type");req.setValue(Self.base.absoluteString,forHTTPHeaderField:"Origin");req.httpBody=try JSONSerialization.data(withJSONObject:body)
        let conf=URLSessionConfiguration.ephemeral;conf.httpShouldSetCookies=false;let valid=(cookieSnapshot ?? cookies).filter{cookie in let p=cookie.path;return (cookie.expiresDate.map{$0>Date()} ?? true) && (url.path==p || url.path.hasPrefix(p.hasSuffix("/") ? p:p+"/"))};req.setValue(HTTPCookie.requestHeaderFields(with:valid)["Cookie"],forHTTPHeaderField:"Cookie");let session=URLSession(configuration:conf,delegate:NoRedirect(),delegateQueue:nil);defer{session.finishTasksAndInvalidate()};let(data,response)=try await session.data(for:req);guard let http=response as? HTTPURLResponse else{throw URLError(.badServerResponse)}
        if sessionPolicy.rejects(status:http.statusCode,generation:generation,cancelled:Task.isCancelled), plan != nil, (path != "last-play" || http.statusCode == 401) {
            let deniedSerial=serial;retire(save:false);emit?(["serial":deniedSerial,"event":"auth-revoked","status":http.statusCode,"paused":true,"ready":false])
            throw URLError(.userAuthenticationRequired)
        }
        guard (200..<300).contains(http.statusCode),let j=try JSONSerialization.jsonObject(with:data) as? [String:Any] else{throw URLError(.badServerResponse)};return j}
    private final class NoRedirect:NSObject,URLSessionTaskDelegate{func urlSession(_ session:URLSession,task:URLSessionTask,willPerformHTTPRedirection response:HTTPURLResponse,newRequest request:URLRequest,completionHandler:@escaping(URLRequest?)->Void){completionHandler(nil)}}
}
