/*
 * R13: EXPERIMENTAL actual AirPlay type112 transport probe.
 * Included into exact v1.2.14 libaltscreen111_gen2.c only when compiled
 * with -DCLUSTERMAP_R13_112_LAB. Requires environment
 * CLUSTERMAP_R13_112_PROBE=1. Without BOTH, no /info or SETUP change.
 *
 * This is not a release feature. It verifies whether an iPhone sends
 * independent original navigation-card packets at all. No video output.
 * In particular, observing packets does NOT prove GaoDe's card content.
 */
#ifndef CLUSTERMAP_CARD_R13_112_PROBE_INC
#define CLUSTERMAP_CARD_R13_112_PROBE_INC

#ifdef CLUSTERMAP_R16_112_TEE_LAB
#include "r16_card_tee.h"
#endif
#define R13_STREAM_CARD 112
#define R13_CARD_UUID "b7e6c5a0-2222-4000-8000-000000000003"
#define R13_CARD_URL ALT_URL_INSTRUCTIONCARD

static pthread_mutex_t r13_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_t r13_thread;
static int r13_thread_started=0,r13_listen_fd=-1,r13_client_fd=-1;
static aes_ctr_storage_t r13_aes;
static int r13_aes_valid=0;
static volatile int r13_stop_flag=0;
static uint64_t r13_session_connection=0;

static int r13_enabled(void)
{
    const char *opt=getenv("CLUSTERMAP_R13_112_PROBE");
    return g_enabled && opt && strcmp(opt,"1")==0;
}
static CFDictionaryRef r13_find112(CFDictionaryRef req)
{
    CFArrayRef a=get_streams(req);
    CFIndex i,n;
    if(!a)return NULL;
    n=p_CFArrayGetCount(a);
    for(i=0;i<n;i++){
        CFDictionaryRef d=(CFDictionaryRef)p_CFArrayGetValueAtIndex(a,i);
        if(stream_type(d)==R13_STREAM_CARD)return d;
    }
    return NULL;
}
static CFMutableDictionaryRef r13_strip112(CFDictionaryRef req)
{
    CFArrayRef a=get_streams(req);
    CFMutableDictionaryRef copy;
    CFMutableArrayRef b;
    CFStringRef streams;
    CFIndex i,n;
    if(!req||!a)return NULL;
    copy=dict_clone(req);
    b=p_CFArrayCreateMutable(NULL,0,p_array_callbacks);
    streams=s_cf("streams");
    if(!copy||!b||!streams){
        if(copy)p_CFRelease(copy);
        if(b)p_CFRelease(b);
        if(streams)p_CFRelease(streams);
        return NULL;
    }
    n=p_CFArrayGetCount(a);
    for(i=0;i<n;i++){
        CFDictionaryRef d=(CFDictionaryRef)p_CFArrayGetValueAtIndex(a,i);
        if(stream_type(d)!=R13_STREAM_CARD)p_CFArrayAppendValue(b,d);
    }
    p_CFDictionarySetValue(copy,streams,b);
    p_CFRelease(streams);
    p_CFRelease(b);
    return copy;
}
static int r13_any_streams(CFDictionaryRef req)
{
    CFArrayRef a=get_streams(req);
    return a&&p_CFArrayGetCount(a)>0;
}
static void r13_stop_receiver(void)
{
    pthread_t t;
    int join=0,fd=-1;
    pthread_mutex_lock(&r13_lock);
    r13_stop_flag=1;
    if(r13_client_fd>=0)(void)shutdown(r13_client_fd,SHUT_RDWR);
    if(r13_listen_fd>=0)(void)shutdown(r13_listen_fd,SHUT_RDWR);
    if(r13_thread_started&&!pthread_equal(pthread_self(),r13_thread)){
        t=r13_thread;r13_thread_started=0;join=1;
    }
    pthread_mutex_unlock(&r13_lock);
    if(join)pthread_join(t,NULL);
    pthread_mutex_lock(&r13_lock);
    fd=r13_listen_fd;r13_listen_fd=-1;
    if(r13_aes_valid){g_aes_ctr_final(&r13_aes);r13_aes_valid=0;}
    memset(&r13_aes,0,sizeof(r13_aes));
    r13_session_connection=0;
    pthread_mutex_unlock(&r13_lock);
    if(fd>=0)close(fd);
}
static void *r13_card_worker(void *arg)
{
    int listener=(int)(intptr_t)arg;
    int fd;
    unsigned configs=0,frames=0,decrypted=0,rejects=0;
    uint64_t bytes=0,probe_stream=0;
    struct alt111_video parsed112;
#ifdef CLUSTERMAP_R16_112_TEE_LAB
    struct r16_card_tee r16tee;
    int r16tee_on=0;
    r16_card_tee_reset(&r16tee);
#endif
    alt111_video_init(&parsed112);
    probe_stream=alt111_video_begin(&parsed112,r13_session_connection);
    fd=accept(listener,NULL,NULL);
    if(fd<0){
        logf_u2("R13 type112 accept unavailable errno=%d",errno);
        alt111_video_destroy(&parsed112);
        return NULL;
    }
    pthread_mutex_lock(&r13_lock);
    r13_client_fd=fd;
    pthread_mutex_unlock(&r13_lock);
#ifdef CLUSTERMAP_R16_112_TEE_LAB
    {
        const char*enable=getenv("CLUSTERMAP_R16_112_TEE");
        if(enable && strcmp(enable,"1")==0 &&
           r16_card_tee_open(&r16tee,&parsed112,R16_CARD_PORT)==0){
            r16tee_on=1;
            logf_u2("R16 experimental 112 Annex-B at 127.0.0.1:%d (lab only)",R16_CARD_PORT);
        }
    }
#endif
    logf_u2("R13 type112 iPhone socket connected, validating encrypted packet flow");
    for(;;){
        AirPlayScreenHeaderCompat h;
        uint8_t *body=NULL;
        int rr;
        if(r13_stop_flag)break;
#ifdef CLUSTERMAP_R16_112_TEE_LAB
        if(r16tee_on)r16_card_tee_pump(&r16tee);
#endif
        rr=read_exact(fd,&h,sizeof(h));
        if(rr<=0)break;
        if(h.bodySize>MAX_SCREEN_BODY){++rejects;break;}
        if(h.bodySize){
            body=(uint8_t*)malloc(h.bodySize);
            if(!body){++rejects;break;}
            rr=read_exact(fd,body,h.bodySize);
            if(rr<=0){free(body);break;}
        }
        if(h.opcode==K_SCREEN_VIDEO_CONFIG){
            if(body && h.bodySize){
                int crc=alt111_video_config(&parsed112,probe_stream,body,h.bodySize);
                if(crc==ALT111_OK||crc==ALT111_RESTART_CONSUMER){
                    ++configs;
                    logf_u2("R13 type112 validated avcC config length=%u",h.bodySize);
                }else ++rejects;
            } else ++rejects;
        }else if(h.opcode==K_SCREEN_VIDEO_FRAME){
            if(!configs){++rejects;}
            else if(body&&h.bodySize){
                OSStatus e;
                pthread_mutex_lock(&r13_lock);
                e=r13_aes_valid?g_aes_ctr_update(&r13_aes,body,h.bodySize,body):-1;
                pthread_mutex_unlock(&r13_lock);
                if(e==K_NO_ERR){
                    uint64_t before=parsed112.source_aus;
                    int parsed=alt111_video_submit(&parsed112,probe_stream,body,h.bodySize,1);
                    ++decrypted; /* CTR call success alone proves NO plaintext integrity. */
                    if(parsed==ALT111_WAIT && parsed112.source_aus>before){
                        ++frames;bytes+=h.bodySize;
                        if(frames<=3 || (frames%30)==0)
                            logf_u2("R13 type112 validated H264 AU count=%u bytes_total=%llu (content still unverified)",
                                    frames,(unsigned long long)bytes);
                    }else{
                        ++rejects;
                        logf_u2("R13 type112 invalid H264 AU rc=%d; AES plaintext NOT confirmed",parsed);
                    }
                } else {++rejects;free(body);break;}
            }
        }else if(h.opcode!=K_SCREEN_KEEPALIVE&&
                 h.opcode!=K_SCREEN_KEEPALIVE_BODY&&
                 h.opcode!=K_SCREEN_IGNORE&&
                 h.opcode!=K_SCREEN_FORCE_KEYFRAME){
            ++rejects;
        }
#ifdef CLUSTERMAP_R16_112_TEE_LAB
        if(r16tee_on)r16_card_tee_pump(&r16tee);
#endif
        free(body);
        /* R13 probe bounded at 120; opt-in R16 lab capped at 30000 AUs. */
#ifdef CLUSTERMAP_R16_112_TEE_LAB
        if((!r16tee_on && frames>=120u)||(r16tee_on&&frames>=30000u)||rejects>=10u)break;
#else
        if(frames>=120u || rejects>=10u)break;
#endif
    }
    logf_u2("R13 type112 SUMMARY connection=%llu avcC=%u ctrCalls=%u validatedH264=%u bytes=%llu rejects=%u (GaoDe content NOT verified)",
            (unsigned long long)r13_session_connection,configs,decrypted,frames,
            (unsigned long long)bytes,rejects);
#ifdef CLUSTERMAP_R16_112_TEE_LAB
    if(r16tee_on){
        logf_u2("R16 loopback summary connects=%u bytes=%llu disconnects=%u errors=%u",
          r16tee.connected,(unsigned long long)r16tee.sent_bytes,r16tee.detached,r16tee.errors);
        r16_card_tee_close(&r16tee);
    }
#endif
    alt111_video_destroy(&parsed112);
    pthread_mutex_lock(&r13_lock);
    if(r13_client_fd==fd)r13_client_fd=-1;
    pthread_mutex_unlock(&r13_lock);
    close(fd);
    return NULL;
}
static int r13_start_receiver(uint64_t connection)
{
    uint8_t key[16]={0},iv[16]={0};
    int port=0,fd=-1,rc;
    if(!connection||!r13_enabled())return -1;
    r13_stop_receiver();
    pthread_mutex_lock(&g_lock);
    if(!g_master_valid){pthread_mutex_unlock(&g_lock);return -1;}
    g_derive_screen(g_master_key,16,connection,key,iv);
    pthread_mutex_unlock(&g_lock);

    pthread_mutex_lock(&r13_lock);
    memset(&r13_aes,0,sizeof(r13_aes));
    rc=g_aes_ctr_init(&r13_aes,key,iv);
    memset(key,0,sizeof(key));memset(iv,0,sizeof(iv));
    if(rc!=K_NO_ERR){pthread_mutex_unlock(&r13_lock);return -1;}
    r13_aes_valid=1;
    /* Inbound iPhone packet listener. It is never reachable from the local tee. */
    fd=bind_listener_ipv4(0,&port,0,1);
    if(fd<0){
        g_aes_ctr_final(&r13_aes);r13_aes_valid=0;
        pthread_mutex_unlock(&r13_lock);
        return -1;
    }
    r13_listen_fd=fd;r13_client_fd=-1;r13_stop_flag=0;
    r13_session_connection=connection;
    if(pthread_create(&r13_thread,NULL,r13_card_worker,(void*)(intptr_t)fd)!=0){
        close(fd);r13_listen_fd=-1;
        g_aes_ctr_final(&r13_aes);r13_aes_valid=0;
        r13_session_connection=0;
        pthread_mutex_unlock(&r13_lock);
        return -1;
    }
    r13_thread_started=1;
    pthread_mutex_unlock(&r13_lock);
    logf_u2("R13 type112 listening dataPort=%d separate AES lifecycle",port);
    return port;
}
static void r13_append_response(CFMutableDictionaryRef response,
                                 CFDictionaryRef request,int port)
{
    CFMutableDictionaryRef sd=NULL;
    CFMutableArrayRef a=NULL;
    CFArrayRef old=NULL;
    CFStringRef key=NULL,kport=NULL,kid=NULL;
    if(!response||!request||port<=0)return;
    key=s_cf("streams");kport=s_cf("dataPort");kid=s_cf("streamID");
    if(!key||!kport||!kid)goto done;
    old=(CFArrayRef)p_CFDictionaryGetValue(response,key);
    a=old?p_CFArrayCreateMutableCopy(NULL,0,old):
          p_CFArrayCreateMutable(NULL,0,p_array_callbacks);
    sd=dict_clone(request);
    if(!a||!sd)goto done;
    p_CFDictionarySetInt64(sd,kport,port);
    p_CFDictionarySetInt64(sd,kid,R13_STREAM_CARD);
    p_CFArrayAppendValue(a,sd);
    p_CFDictionarySetValue(response,key,a);
done:
    if(sd)p_CFRelease(sd);
    if(a)p_CFRelease(a);
    if(kid)p_CFRelease(kid);
    if(kport)p_CFRelease(kport);
    if(key)p_CFRelease(key);
}
static void r13_advertise(CFMutableArrayRef displays,CFDictionaryRef alt111)
{
    CFMutableDictionaryRef card;
    if(!r13_enabled()||!displays||!alt111)return;
    /*
     * R15: 112 must not overlay the stock 111 composite (map + white card).
     * The upstream AirPlay /info has already appended this mutable 111
     * dictionary to 'displays'. Only in explicitly enabled R13 lab mode,
     * request card-free 111 before offering the card-only second display.
     * Otherwise we would display the original white card AND a smaller copy.
     * This is a request to iOS, NOT evidence that GaoDe honors the two URLs.
     */
    set_str((CFMutableDictionaryRef)alt111,"initialURL",ALT_URL_MAP);
    remove_key((CFMutableDictionaryRef)alt111,"altScreenSuggestUIURLs");
    card=dict_clone(alt111);
    if(!card)return;
    set_i64(card,"type",R13_STREAM_CARD);
    set_str(card,"uuid",R13_CARD_UUID);
    set_str(card,"initialURL",R13_CARD_URL);
    /* The extra display must be dedicated to instructioncard, never map/full. */
    remove_key(card,"altScreenSuggestUIURLs");
    p_CFArrayAppendValue(displays,card);
    p_CFRelease(card);
    logf_u2("R15 EXPERIMENTAL /info requested 111 map-only + 112 card-only UUID=%s (not phone-verified)",R13_CARD_UUID);
}
static void r13_setup(AirPlayReceiverSessionRef session,CFDictionaryRef request,
                      CFMutableDictionaryRef response)
{
    CFDictionaryRef sd;
    uint64_t cid;
    int port;
    (void)session;
    if(!r13_enabled()||!response)return;
    sd=r13_find112(request);
    if(!sd)return;
    cid=stream_connection_id(sd);
    port=r13_start_receiver(cid);
    if(port>0){
        r13_append_response(response,sd,port);
        logf_u2("R13 stream112 SETUP accepted, port=%d (probe only)",port);
    }else{
        logf_u2("R13 stream112 SETUP not ready; no false response");
    }
}
static void r13_stock_teardown_forward(AirPlayReceiverSessionRef session,
       CFDictionaryRef request,OSStatus reason,Boolean *outDone)
{
    CFMutableDictionaryRef filtered=NULL;
    if(r13_enabled() && request && r13_find112(request)){
        filtered=r13_strip112(request);
        if(filtered){
            if(r13_any_streams(filtered))
                call_stock_teardown(session,filtered,reason,outDone);
            else if(outDone)*outDone=0;
            p_CFRelease(filtered);
            return;
        }
    }
    call_stock_teardown(session,request,reason,outDone);
}
#endif /* CLUSTERMAP_CARD_R13_112_PROBE_INC */
