/* SPDX-License-Identifier: GPL-3.0-only
 * Execute exact previously built ARM library under QNX, using the existing
 * two-ended integration fixture. No OEM device, radio or MFi simulation. */
#include "../../integration/mu1005_si_native/inner_iap2/mm_peer.h"
#include "../../integration/mu1005_si_native/inner_iap2/inner_socket.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#define FN(n) static __typeof__(&n) p_##n
FN(mu_mm_peer_adopt);FN(mu_mm_peer_tick);FN(mu_mm_peer_send);
FN(mu_mm_peer_recv);FN(mu_mm_peer_release);FN(mu_inner_socket_adopt);
FN(mu_inner_socket_tick);FN(mu_inner_from_control);
#define mu_mm_peer_adopt p_mu_mm_peer_adopt
#define mu_mm_peer_tick p_mu_mm_peer_tick
#define mu_mm_peer_send p_mu_mm_peer_send
#define mu_mm_peer_recv p_mu_mm_peer_recv
#define mu_mm_peer_release p_mu_mm_peer_release
#define mu_inner_socket_adopt p_mu_inner_socket_adopt
#define mu_inner_socket_tick p_mu_inner_socket_tick
#define mu_inner_from_control p_mu_inner_from_control
#define main fixture_main
#include "../../integration/mu1005_si_native/inner_iap2/test_mm_peer.c"
#undef main
static int bind_symbol(void *h,const char *name,void *slot,size_t n){
 void *p=dlsym(h,name);
 if(!p||n!=sizeof p){fprintf(stderr,"SYMBOL_FAIL:%s\n",name);return -1;}
 memcpy(slot,&p,n);return 0;
}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
 puts("QNX_INNER_STAGE:socketpair preflight");
 int f[2];
 if(socketpair(AF_UNIX,SOCK_STREAM,0,f)<0){perror("QNX_SOCKETPAIR");return 2;}
 close(f[0]);close(f[1]);
 puts("QNX_INNER_STAGE:real ARM library load");
 void *h=dlopen("/proc/boot/libmu1005_inner_iap2_review.so",RTLD_NOW|RTLD_LOCAL);
 if(!h){fprintf(stderr,"QNX_INNER_DLOPEN_FAIL:%s\n",dlerror());return 3;}
#define B(n) if(bind_symbol(h,#n,&p_##n,sizeof p_##n))return 4
 B(mu_mm_peer_adopt);B(mu_mm_peer_tick);B(mu_mm_peer_send);
 B(mu_mm_peer_recv);B(mu_mm_peer_release);B(mu_inner_socket_adopt);
 B(mu_inner_socket_tick);B(mu_inner_from_control);
#undef B
 puts("QNX_INNER_STAGE:two-ended fixture");
 if(fixture_main()!=0)return 5;
 if(dlclose(h)!=0)return 6;
 puts("QNX_INNER_BIDIRECTIONAL_REAL_RUNTIME_PASS_NO_OEM");
 return 0;
}
