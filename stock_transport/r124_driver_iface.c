#include "r124_driver_iface.h"
#include <errno.h>
#include <string.h>
static struct { R124Iface iface; R123Transport *transport; R124Proof proof; void *device; int state,notifying; } r;
static int live(void){
 if(r.state!=2||!r.transport)return 0;
 if(!r.transport->active||r.transport->failed||r.proof.authorized(r.proof.context,r.device,r.transport->generation)!=1){r123_transport_revoke(r.transport);r.state=3;return 0;}
 return 1;
}
static int init(R124Iface *i,const char *path,const char *opts){
 (void)path;(void)opts;
 if(i!=&r.iface||i->owner!=r.device||r.state!=1)return EINVAL;
 if(!r.transport->active||r.transport->failed||r.proof.authorized(r.proof.context,r.device,r.transport->generation)!=1){r123_transport_revoke(r.transport);r.state=3;return EACCES;}
 r.state=2;return 0;
}
static int link(R124Iface *i,void *out){
 /* Original driver defaults: first 16 bytes, deterministic padding. */
 static const uint8_t defaults[16]={1,0,0xe8,3,20,0,30,1,1,0,128,0,1,0,128,0};
 if(i!=&r.iface||!out)return EINVAL;
 /* link_create ignores result. The owner must ensure driver is not started
  * after revoke; never leave uninitialized stack on an unexpected late query. */
 memcpy(out,defaults,sizeof defaults);return live()?0:EACCES;
}
static int type(R124Iface *i,uint8_t *out){if(i!=&r.iface||!out||!live())return EACCES;*out=3;return 0;}
static int caps(R124Iface *i,unsigned key,void *out,unsigned n){(void)i;(void)key;(void)out;(void)n;return 0;}
static int audio(R124Iface *i){(void)i;return 0;}
static int tx(void *d,const void *data,unsigned n){
 if(d!=r.device||!live()||!data||!n||n>WC_RF_MAX){errno=EINVAL;return -1;}
 int rc=r123_transport_send(r.transport,data,n,250);
 if(rc!=(int)n){r123_transport_revoke(r.transport);r.state=3;errno=EIO;return -1;}
 return rc;
}
static int rx(void *d,void *data,unsigned n,int ms,void *opaque){
 (void)opaque;if(d!=r.device||!live()||!data||!n||ms!=0){errno=EINVAL;return -1;}
 /* Pinned original transport_receive passes zero timeout and fifth arg. */
 int rc=r123_transport_recv(r.transport,data,n,0);
 if(rc<0){r.state=3;errno=EIO;}return rc;
}
int r124_driver_prepare(R123Transport *t,void *device,const R124Proof *p,R124Iface **out){
 if(!out)return -1;
 *out=NULL;
 if(r.state||!t||!device||!p||!p->authorized||!t->opened||!t->active||t->failed||p->authorized(p->context,device,t->generation)!=1)return -1;
 r.transport=t;r.device=device;r.proof=*p;r.state=1;
 r.iface=(R124Iface){.name="ipod_transport",.owner=device,.init=init,.link=link,.type=type,.caps=caps,.audio=audio,.send=tx,.recv=rx};
 *out=&r.iface;return 0;
}
int r124_driver_poll(void (*input_ready)(void *,void *),void *context){
 if(!input_ready||r.notifying||!live())return -1;
 int rc=r123_transport_data_ready(r.transport);
 if(rc<0){r.state=3;return -1;}if(!rc)return 0;
 r.notifying=1;input_ready(context,r.device);r.notifying=0;
 return live()?1:-1;
}
int r124_driver_revoke(void){if(!r.state)return -1;r123_transport_revoke(r.transport);r.state=3;return 0;}
