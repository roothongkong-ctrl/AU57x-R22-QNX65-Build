/* Injected by iOS only. The website keeps the real HTMLAudioElement and its event listeners. */
(function (root) {
  'use strict';
  const bridge=root.NASNativeMusic;
  if (!bridge || bridge.version!==2) return;
  root.NASNativeAudio = function(audio, plan, onTrack, onRecent) {
    const proto=root.HTMLMediaElement.prototype;
    const desc=k=>Object.getOwnPropertyDescriptor(proto,k);
    const get=k=>desc(k).get.call(audio), set=(k,v)=>desc(k).set.call(audio,v);
    const original={play:audio.play.bind(audio),pause:audio.pause.bind(audio),load:audio.load.bind(audio),get:audio.getAttribute.bind(audio),remove:audio.removeAttribute.bind(audio)};
    let active=false, src='', serial=0, pending=null, state={}, currentSlot=null, started=false, requestedSeek=null;
    const send=(action,extra={})=>bridge.send({action,serial,...extra});
    const event=name=>audio.dispatchEvent(new Event(name));
    const reject=()=>{if(pending){pending.reject(new Error('播放请求已取消'));clearTimeout(pending.timer);pending=null;}};
    const clear=()=>{if(active){send('stop');reject();serial++;active=false;state={};src='';currentSlot=null;started=false;requestedSeek=null;}};
    const urlOK=value=>{try{const u=new URL(value,root.location.href);return u.origin===root.location.origin&&['/api/music/play-file','/api/music/private-audio'].includes(u.pathname);}catch(_){return false;}};
    Object.defineProperty(audio,'src',{configurable:true,get:()=>active?new URL(src,root.location.href).href:get('src'),set:value=>{
      clear(); if(!urlOK(value)){set('src',value);return;}
      original.pause();original.remove('src');original.load(); active=true;src=String(value);state={paused:true,time:0,duration:null,ready:false};
    }});
    audio.getAttribute=function(k){return active&&k.toLowerCase()==='src'?src:original.get(k);};
    audio.removeAttribute=function(k){if(k.toLowerCase()==='src')clear();return original.remove(k);};
    audio.load=function(){if(!active)original.load();};
    for(const [k,read] of Object.entries({paused:()=>state.paused,ended:()=>!!state.ended,error:()=>state.error?{message:state.error,code:2}:null,readyState:()=>state.ready?4:0,seeking:()=>!!state.seeking,duration:()=>state.duration??NaN,currentTime:()=>state.time||0})) {
      Object.defineProperty(audio,k,{configurable:true,get:()=>active?read():get(k),...(k==='currentTime'?{set:value=>{if(active){const t=Number(value);if(!Number.isFinite(t)||t<0)return;state.time=t;if(started){send('seek',{seconds:t});}else{requestedSeek=t;}}else set(k,value);}}:{})});
    }
    audio.play=function(){
      if(!active)return original.play();
      if(pending)return pending.promise;
      if(started && state.ready && !state.error){send('resume');return Promise.resolve();}
      const payload=plan(src);if(!payload)return Promise.reject(new Error('原生播放参数缺失'));
      if(started)send('stop');serial++;started=true;state.error=null;state.ready=false;state.ended=false;
      if(requestedSeek!==null)payload.position=requestedSeek;requestedSeek=null;
      let resolve,reject;const promise=new Promise((a,b)=>{resolve=a;reject=b;});
      const timer=setTimeout(()=>{if(pending?.promise!==promise)return;pending=null;send('stop');serial++;started=false;state.ready=false;state.error='原生播放超时';state.paused=true;reject(new Error(state.error));event('error');},14000);
      pending={promise,resolve,reject,timer};currentSlot=payload.start;send('start',payload);return promise;
    };
    audio.pause=function(){if(!active)return original.pause();if(pending){send('stop');reject();serial++;started=false;state={paused:true,ready:false,time:0,duration:null};}else{send('pause');state.paused=true;}event('pause');};
    root.NASNativeAudioState=function(msg){
      if(!active||msg.serial!==serial)return;
      if(msg.event==='auth-revoked'){
        clear();original.pause();original.remove('src');original.load();
        root.dispatchEvent(new CustomEvent('nas-native-auth-revoked',{detail:{status:msg.status===401?401:403}}));
        return;
      }
      if(msg.event==='recent'){if(Array.isArray(msg.rows))onRecent?.(msg.rows);return;}
      if(msg.track){const changed=currentSlot!==msg.track.slot||new URL(src,root.location.href).href!==msg.track.url;currentSlot=msg.track.slot; if(changed){src=msg.track.url;state.error=null;state.ready=false;}onTrack(msg.track);}
      const wasReady=state.ready, wasPaused=state.paused;
      state={...state,...msg};
      if(msg.event==='error'){state.ready=false;state.paused=true;}
      if(state.ready&&!wasReady){event('loadedmetadata');event('durationchange');}
      if(msg.event==='playing'){
        state.error=null;
        if(pending){clearTimeout(pending.timer);pending.resolve();pending=null;}
        if(wasPaused)event('play');event('playing');
      }else if(msg.event==='error'){
        if(pending){clearTimeout(pending.timer);pending.reject(new Error(msg.error||'播放失败'));pending=null;}event('error');
      }else if(msg.event==='pause'||msg.event==='seeked'||msg.event==='waiting')event(msg.event);
      else event('timeupdate');
    };
    return {active:()=>active, update:()=>{if(active){const p=plan(src);if(p)send('policy',{mode:p.mode,sleepMinutes:p.sleepMinutes,sleepUntil:p.sleepUntil});}}, queueChanged:()=>{if(active&&state.ready&&!pending&&Number.isInteger(currentSlot)){const p=plan(src);if(p)send('queue',{...p,expectedSlot:currentSlot});}}, clear};
  };
})(window);
