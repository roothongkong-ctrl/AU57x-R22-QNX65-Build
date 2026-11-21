/* QNX 6.5 ARMv7: test loader only, never establish an OEM session. */
#include <stdio.h>
#include <dlfcn.h>
#include <stddef.h>

#include "../project/source/legacy_mu1005/bluetooth/mu1005_ipc_stream.h"
#include <string.h>
static AuRfIngress r24_rf;
static AuIpcStream r24_ipc;
static unsigned char sent[8];
static size_t sent_n;
static unsigned sends;
static int r24_sdk(void *ctx,const uint8_t *p,size_t n){
 (void)ctx;
 if(!p||!n||n>2||sent_n+n>sizeof sent)return -1;
 memcpy(sent+sent_n,p,n);sent_n+=n;++sends;return 0;
}
static int r24_contract(void*h){
 typedef void(*init_f)(AuRfIngress*,AuRfSdkSend,void*);
 typedef void(*ipc_init_f)(AuIpcStream*,AuRfIngress*);
 typedef int(*connect_f)(AuRfIngress*,const uint8_t*);
 typedef int(*mtu_f)(AuRfIngress*,size_t);
 typedef int(*feed_f)(AuIpcStream*,const uint8_t*,size_t);
 typedef int(*peek_f)(AuIpcStream*,const uint8_t**,size_t*);
 typedef int(*commit_f)(AuIpcStream*,size_t);
 typedef int(*tx_f)(AuRfIngress*,int);
 typedef int(*recv_f)(AuRfIngress*,const void*,size_t);
 typedef int(*disc_f)(AuRfIngress*);
 typedef int(*raw_f)(AuRfIngress*,const uint8_t*,size_t);
 typedef void(*lost_f)(AuIpcStream*);
 init_f init=(init_f)dlsym(h,"au_rf_init");
 ipc_init_f ipcinit=(ipc_init_f)dlsym(h,"au_ipc_init");
 connect_f conn=(connect_f)dlsym(h,"au_rf_connected");
 mtu_f mtu=(mtu_f)dlsym(h,"au_rf_set_max_payload");
 feed_f feed=(feed_f)dlsym(h,"au_ipc_feed");
 peek_f peek=(peek_f)dlsym(h,"au_ipc_peek");
 commit_f commit=(commit_f)dlsym(h,"au_ipc_commit");
 tx_f done=(tx_f)dlsym(h,"au_rf_send_complete");
 recv_f recv=(recv_f)dlsym(h,"au_rf_received");
 disc_f disc=(disc_f)dlsym(h,"au_rf_disconnected");
 raw_f raw=(raw_f)dlsym(h,"au_rf_daemon_data");
 lost_f lost=(lost_f)dlsym(h,"au_ipc_socket_lost");
 if(!init||!ipcinit||!conn||!mtu||!feed||!peek||!commit||!done||!recv||!disc||!raw||!lost)return 1;
 const uint8_t peer[6]={1,2,3,4,5,6};
 const uint8_t wire[13]={2,0,5,0,1,0,0,0,'h','e','l','l','o'};
 const uint8_t *p=0;size_t n=0;
 init(&r24_rf,r24_sdk,0);ipcinit(&r24_ipc,&r24_rf);
 if(conn(&r24_rf,peer)!=0||r24_rf.generation!=1)return 2;
 if(peek(&r24_ipc,&p,&n)!=1||n!=14||p[0]!=1||memcmp(p+8,peer,6))return 3;
 if(commit(&r24_ipc,4)!=0||peek(&r24_ipc,&p,&n)!=1||n!=10||commit(&r24_ipc,10)!=0)return 4;
 if(mtu(&r24_rf,2)!=0)return 5;
 if(feed(&r24_ipc,wire,3)!=0||sends||feed(&r24_ipc,wire+3,2)!=0||sends||
    feed(&r24_ipc,wire+5,8)!=0||sends!=1)return 6;
 if(done(&r24_rf,1)!=0||sends!=2||done(&r24_rf,1)!=0||sends!=3||
    done(&r24_rf,1)!=0||r24_rf.tx_busy||sent_n!=5||memcmp(sent,"hello",5))return 7;
 if(recv(&r24_rf,"abc",3)!=0||peek(&r24_ipc,&p,&n)!=1||n!=11||p[0]!=2||
    commit(&r24_ipc,n)!=0||r24_ipc.rx_frames_committed!=1)return 8;
 if(disc(&r24_rf)!=0||r24_rf.generation!=2||peek(&r24_ipc,&p,&n)!=1||
    n!=8||p[0]!=3||commit(&r24_ipc,n)!=0)return 9;
 if(conn(&r24_rf,peer)!=0||r24_rf.generation!=3||mtu(&r24_rf,2)!=0||
    raw(&r24_rf,wire,sizeof wire)==0)return 10;
 lost(&r24_ipc);
 if(!r24_rf.failed||!r24_ipc.failed||r24_rf.connected)return 11;
 puts("AU57X_R24_BT_FRAMED_QNX_OK");return 0;
}


int main(void) {
    const char *name = "libau57x_r24_native_qnx_REVIEW_ONLY.so";
    void *h = dlopen(name, RTLD_NOW);
    if (!h) {
        const char *err = dlerror();
        printf("AU57X_DLOPEN_FAILED: %s\n", err ? err : "unknown");
        return 11;
    }
    typedef int (*ready_fn)(const void *);
    dlerror();
    ready_fn f = (ready_fn)dlsym(h, "cp_ho_media_ready");
    const char *err = dlerror();
    if (err || !f) {
        printf("AU57X_DLSYM_FAILED: %s\n", err ? err : "not found");
        dlclose(h);
        return 12;
    }
    /* NULL must be rejected by the review-only state handler. */
    if (f(NULL) != 0) {
        puts("AU57X_SAFE_NULL_REJECT_FAILED");
        dlclose(h);
        return 13;
    }
    typedef int (*csm_fn)(const unsigned char *, size_t);
    typedef int (*iface_fn)(const char *, char *, size_t);
    dlerror();
    csm_fn validate = (csm_fn)dlsym(h, "mu_sem_csm");
    iface_fn iface = (iface_fn)dlsym(h, "mu_wifi_interface_for_ipv4");
    void *service = dlsym(h, "mu_cprx_open_backend_si");
    err = dlerror();
    if (err || !validate || !iface || !service) {
        printf("AU57X_R23_MODULE_SYMBOL_MISSING: %s\n", err ? err : "empty");
        dlclose(h);
        return 15;
    }
    const unsigned char csm[] = {0x40, 0x40, 0x00, 0x06, 0x50, 0x00};
    if (validate(csm, sizeof csm) != 1 || validate(csm, 5) != 0) {
        puts("AU57X_R23_CSM_REJECTION_FAILED");
        dlclose(h);
        return 16;
    }
    char interface_name[17];
    if (iface("127.0.0.1", interface_name, sizeof interface_name) != -1) {
        puts("AU57X_R23_LOOPBACK_REJECTION_FAILED");
        dlclose(h);
        return 17;
    }
    puts("AU57X_R23_MODULES_LOADED_OK");
    typedef int (*socket_fn)(void *, int, int);
    socket_fn socket_step=(socket_fn)dlsym(h,"au_socket_step");
    void *socket_done=dlsym(h,"au_socket_tx_complete");
    if (!socket_step || !socket_done || socket_step(NULL,-1,0)!=-2) {
        puts("AU57X_R24_SOCKET_ADAPTER_MISSING");
        dlclose(h); return 18;
    }
    puts("AU57X_R24_SOCKET_ADAPTER_LOADED_OK");
    /* New R36 radio policy is a pure model. No Wi-Fi hardware is accessed. */
    if (!dlsym(h,"cp_radio_begin") || !dlsym(h,"cp_radio_select") ||
        !dlsym(h,"cp_radio_association_result")) {
        puts("AU57X_R36_RADIO_MODEL_SYMBOL_MISSING");dlclose(h);return 19;
    }
    puts("AU57X_R36_RADIO_MODEL_LOADED_OK");
    if (!dlsym(h,"cp_mu_decode_snapshot")) {
        puts("AU57X_R38_CALLBACK_PARSER_MISSING");dlclose(h);return 19;
    }
    puts("AU57X_R38_CALLBACK_PARSER_LOADED_OK");
    if (!dlsym(h,"cp_mu_bridge_feed") || !dlsym(h,"cp_mu_bridge_init")) {
        puts("AU57X_R39_BRIDGE_SYMBOL_MISSING");dlclose(h);return 19;
    }
    puts("AU57X_R39_BRIDGE_LOADED_OK");
    /* Stock MFi is deliberately NOT invoked inside QEMU. */
    if (!dlsym(h,"cp_stock_mfi_bind") || !dlsym(h,"cp_stock_mfi_sign") ||
        !dlsym(h,"cp_auth_receive") || !dlsym(h,"cp_auth_tx_confirm") ||
        !dlsym(h,"cp_step04_dispatch")) {
       puts("AU57X_R41_STOCK_MFI_AUTH_SYMBOL_MISSING");
       dlclose(h);return 19;
    }
    puts("AU57X_R41_STOCK_MFI_AUTH_SYMBOL_LOADED_OK");
    {int test=r24_contract(h);if(test){printf("AU57X_R24_CONTRACT_FAIL=%d\n",test);dlclose(h);return 20+test;}}
    if (dlclose(h) != 0) {
        puts("AU57X_DLCLOSE_FAILED");
        return 14;
    }
    puts("AU57X_RUNTIME_DLOPEN_OK");
    return 0;
}
