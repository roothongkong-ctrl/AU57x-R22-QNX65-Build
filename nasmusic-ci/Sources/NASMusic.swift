import SwiftUI
import WebKit
import UIKit

@main struct NASMusicApp: App { var body: some Scene { WindowGroup { MusicPage().ignoresSafeArea(edges:.bottom) } } }
@MainActor struct MusicPage:UIViewRepresentable {
    func makeCoordinator()->Coordinator{Coordinator()}
    func makeUIView(context:Context)->WKWebView{
        let c=WKWebViewConfiguration();c.websiteDataStore = .default();c.allowsInlineMediaPlayback=true;c.mediaTypesRequiringUserActionForPlayback=[]
        c.userContentController.add(context.coordinator,name:"nativeMusic")
        let handshake="window.NASNativeMusic={version:2,send:function(m){window.webkit.messageHandlers.nativeMusic.postMessage(m);}};"
        let adapter=Bundle.main.url(forResource:"native-music",withExtension:"js").flatMap{try? String(contentsOf:$0,encoding:.utf8)}
        if let adapter{c.userContentController.addUserScript(WKUserScript(source:handshake+"\n"+adapter,injectionTime:.atDocumentStart,forMainFrameOnly:true))}
        let view=WKWebView(frame:.zero,configuration:c);view.navigationDelegate=context.coordinator;context.coordinator.attach(view);view.allowsBackForwardNavigationGestures=true
        view.load(URLRequest(url:NativeQueue.base.appendingPathComponent("music/"),cachePolicy:.reloadRevalidatingCacheData));return view
    }
    func updateUIView(_ uiView:WKWebView,context:Context){}
    @MainActor final class Coordinator:NSObject,WKNavigationDelegate,WKScriptMessageHandler{
        private let queue=NativeQueue();private var deferredDenial:[String:Any]?;private var requestID=0;private var pendingStart:NativeQueue.Plan?;private var serial:Int?;private weak var web:WKWebView?;private var activeObserver:NSObjectProtocol?, backgroundObserver:NSObjectProtocol?
        func attach(_ web:WKWebView){self.web=web;queue.emit={[weak self] message in self?.emit(message)};activeObserver=NotificationCenter.default.addObserver(forName:UIApplication.didBecomeActiveNotification,object:nil,queue:.main){[weak self] _ in DispatchQueue.main.async{self?.queue.setBackground(false);self?.deliverDenial();self?.queue.refresh()}};backgroundObserver=NotificationCenter.default.addObserver(forName:UIApplication.didEnterBackgroundNotification,object:nil,queue:.main){[weak self] _ in DispatchQueue.main.async{self?.queue.setBackground(true)}}}
        deinit{if let activeObserver{NotificationCenter.default.removeObserver(activeObserver)};if let backgroundObserver{NotificationCenter.default.removeObserver(backgroundObserver)}}
        private func deliverDenial(){if let message=deferredDenial{emit(message)}}
        private func emit(_ message:[String:Any]){if message["event"] as? String == "auth-revoked"{deferredDenial=message};guard UIApplication.shared.applicationState == .active,let web,let url=web.url,valid(url),let data=try? JSONSerialization.data(withJSONObject:message),let json=String(data:data,encoding:.utf8) else{return};if message["event"] as? String == "auth-revoked"{deferredDenial=nil};web.evaluateJavaScript("window.NASNativeAudioState?.(\(json))",completionHandler:nil)}
        private func valid(_ u:URL)->Bool{u.scheme=="https"&&u.host==NativeQueue.base.host&&u.port==NativeQueue.base.port&&u.path.hasPrefix("/music/")}
        func userContentController(_ controller:WKUserContentController,didReceive message:WKScriptMessage){guard message.frameInfo.isMainFrame,let web=message.webView,let u=web.url,valid(u),message.frameInfo.securityOrigin.host==NativeQueue.base.host,message.frameInfo.securityOrigin.protocol=="https",message.frameInfo.securityOrigin.port==NativeQueue.base.port,let b=message.body as? [String:Any],let action=b["action"] as? String,let s=b["serial"] as? Int else{return}
            if action=="start"{guard deferredDenial==nil else{deliverDenial();return};guard let d=try? JSONSerialization.data(withJSONObject:b),let p=try? JSONDecoder().decode(NativeQueue.Plan.self,from:d) else{emit(["serial":s,"event":"error","error":"原生播放参数不兼容","paused":true]);return};requestID+=1;let r=requestID;serial=s;queue.stop();pendingStart=p;web.configuration.websiteDataStore.httpCookieStore.getAllCookies{[weak self] cookies in DispatchQueue.main.async{guard let self,self.requestID==r,self.serial==s,let start=self.pendingStart else{return};self.pendingStart=nil;do{try self.queue.start(start,serial:s,cookies:cookies)}catch{self.queue.stop();self.emit(["serial":s,"event":"error","error":"原生音频启动失败","paused":true,"ready":false])}}};return}
            guard serial==s else{return};switch action{case "queue":if let expected=b["expectedSlot"] as? Int,let data=try? JSONSerialization.data(withJSONObject:b),let p=try? JSONDecoder().decode(NativeQueue.Plan.self,from:data){queue.replaceQueue(p,expectedSlot:expected)};case "stop":requestID+=1;pendingStart=nil;serial=nil;queue.stop();case "pause":requestID+=1;pendingStart=nil;queue.pause();case "resume":queue.resume();case "seek":if let t=b["seconds"] as? Double,let target=PlaybackInput.seek(t){if pendingStart != nil{pendingStart?.position=target}else{queue.seek(target)}};case "policy":if let mode=b["mode"] as? String,let minutes=b["sleepMinutes"] as? Int,let until=b["sleepUntil"] as? Double{queue.policy(mode:mode,minutes:minutes,until:until)};default:break}
        }
        func webView(_ webView:WKWebView,didStartProvisionalNavigation navigation:WKNavigation!){requestID+=1;pendingStart=nil;serial=nil;deferredDenial=nil;queue.stop()}
        func webView(_ webView:WKWebView,didFailProvisionalNavigation navigation:WKNavigation!,withError error:Error){guard (error as NSError).code != NSURLErrorCancelled else{return};let a=UIAlertController(title:"无法连接音乐网站",message:error.localizedDescription,preferredStyle:.alert);a.addAction(UIAlertAction(title:"重试",style:.default){_ in webView.load(URLRequest(url:NativeQueue.base.appendingPathComponent("music/"),cachePolicy:.reloadRevalidatingCacheData))});webView.window?.rootViewController?.present(a,animated:true)}
    }
}
