#include "../project/source/legacy_mu1005/bluetooth/native_event.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int certified(void *p){return p!=0;}
static int dispatched;
void au_ipc_socket_lost(AuIpcStream *s){(void)s;}
int ir5_event(InboundR5*b,void*ch,const void*raw,unsigned code,unsigned status,const uint8_t*data,unsigned len,uint32_t packet){
 assert(b&&b->initialized&&ch==&b->service.rf&&raw&&code==4&&status==0);
 assert(data==0&&len==0&&packet==0);dispatched++;return 7;
}
int main(void){
 InboundR5 b;Mu38PeerOps peer;int owner=1;uint8_t raw[16]={4};
 memset(&b,0,sizeof b);memset(&peer,0,sizeof peer);
 b.initialized=1;b.vendor.opaque=&owner;b.vendor.certify=certified;
 peer.opaque=&owner;
 assert(mu38_dispatch(&b,&peer,&b.service.rf,raw)==7);
 assert(dispatched==1);
 peer.opaque=0;
 assert(mu38_dispatch(&b,&peer,&b.service.rf,raw)==-1);
 assert(dispatched==1);
 puts("MU38 native event resolver context binding: PASS");
 return 0;
}
