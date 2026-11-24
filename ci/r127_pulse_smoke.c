/* QNX kernel dispatch proof only. Uses own dispatch and inert loader fixture.
 * Does not run OEM code or establish an OEM owner / MFi session. */
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "r125_module.h"
typedef struct { int code, count, error; int (*state)(void), (*stop)(void); } Probe;
static int on_pulse(message_context_t *ctp,int code,unsigned flags,void *handle){
 (void)ctp;(void)flags;Probe *p=handle;
 if(code!=p->code||p->count>1||p->state()!=R127_WAITING){p->error=1;return 0;}
 if(p->count==1&&(p->stop()!=0||p->state()!=R125_RETIRED))p->error=1;
 ++p->count;return 0;
}
int main(void){
 alarm(15);
 void *h=dlopen("ipod-transport-au57xbt-REVIEW.so",RTLD_NOW|RTLD_LOCAL);
 if(!h){puts(dlerror());return 1;}
 R125Module *m=dlsym(h,"ipod_module");Probe p={0};void *sym=dlsym(h,"r125_state");
 if(!m||!m->interfaces||!sym||sizeof p.state!=sizeof sym)return 2;
 memcpy(&p.state,&sym,sizeof p.state);sym=dlsym(h,"r125_stop");if(!sym)return 3;memcpy(&p.stop,&sym,sizeof p.stop);
 int fixture=0;R124Iface *i=m->interfaces;i->owner=&fixture;
 if(i->init(i,"/dev/ipod","review-deferred")||p.state()!=R127_WAITING||i->send(&fixture,"x",1)!=-1)return 4;
 dispatch_t *d=dispatch_create();if(!d)return 5;
 p.code=pulse_attach(d,MSG_FLAG_ALLOC_PULSE,0,on_pulse,&p);if(p.code<0)return 6;
 int coid=message_connect(d,MSG_FLAG_SIDE_CHANNEL);if(coid<0)return 7;
 dispatch_context_t *ctx=dispatch_context_alloc(d);if(!ctx)return 8;
 for(int n=0;n<2;++n){if(MsgSendPulse(coid,-1,p.code,n)<0)return 9;dispatch_context_t *next=dispatch_block(ctx);if(!next)return 10;ctx=next;if(dispatch_handler(ctx)<0||p.error||p.count!=n+1)return 11;}
 if(p.state()!=R125_RETIRED||i->send(&fixture,"x",1)!=-1)return 12;
 if(ConnectDetach(coid)<0||pulse_detach(d,p.code,0)<0)return 13;
 dispatch_context_free(ctx);if(dispatch_destroy(d)<0)return 14;
 dlclose(h);alarm(0);puts("AU57X_R127_QNX_PULSE_DEFERRED_LOADER_OK_NO_OEM");return 0;
}
