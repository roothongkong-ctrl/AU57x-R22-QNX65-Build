#include "r125_module.h"
#include <errno.h>
#include <string.h>
static struct {R123Transport *transport;R125Owner owner;R124Iface *inner;void *device;int state,entered,dispatching;} s;
static R124Iface iface;
static int context_ok(void){return s.device&&s.owner.owner_context&&s.owner.owner_context(s.owner.context,s.device)==1;}
static int authorized(void){return context_ok()&&s.transport&&s.transport->active&&!s.transport->failed&&s.owner.authorized(s.owner.context,s.device,s.transport->generation)==1;}
static int proof(void *p,void *device,uint32_t gen){(void)p;return device==s.device&&s.transport&&gen==s.transport->generation&&authorized();}
static void revoke(void){if(s.inner)r124_driver_revoke();else if(s.transport)r123_transport_revoke(s.transport);s.state=R125_STOP_REQUIRED;}
static int available(void){if(s.state!=R125_STARTING&&s.state!=R125_RUNNING)return 0;if(!authorized()){revoke();return 0;}return 1;}
static int init(R124Iface*i,const char*path,const char*opts){
 if(i!=&iface||!i->owner)return EINVAL;
 /* Stock loader invokes init BEFORE its dispatch pool starts. The explicit
  * review option allows inert init, never starts the driver or trusts peer IPC. */
 if(s.state==R125_EMPTY&&path&&opts&&!strcmp(path,"/dev/ipod")&&!strcmp(opts,"review-deferred")){s.device=i->owner;s.state=R127_WAITING;return 0;}
 if(s.state!=R125_ARMED)return EINVAL;
 s.device=i->owner;if(!authorized()){revoke();return EACCES;}
 R124Proof p={NULL,proof};
 if(r124_driver_prepare(s.transport,s.device,&p,&s.inner)||s.inner->init(s.inner,path,opts)){revoke();return EIO;}
 s.state=R125_BOUND;return 0;
}
static int link(R124Iface*i,void*out){static const uint8_t defaults[16]={1,0,0xe8,3,20,0,30,1,1,0,128,0,1,0,128,0};if(i!=&iface||!out)return EINVAL;memcpy(out,defaults,sizeof defaults);if(!available())return EACCES;return s.inner->link(s.inner,out);}
static int type(R124Iface*i,uint8_t*out){if(i!=&iface||!out||!available())return EACCES;return s.inner->type(s.inner,out);}
static int caps(R124Iface*i,unsigned k,void*out,unsigned n){if(i!=&iface||!available())return 0;return s.inner->caps(s.inner,k,out,n);}
static int audio(R124Iface*i){if(i!=&iface||!available())return 0;return s.inner->audio(s.inner);}
static int tx(void*d,const void*p,unsigned n){if(d!=s.device||!available()){errno=EACCES;return -1;}return s.inner->send(d,p,n);}
static int rx(void*d,void*p,unsigned n,int ms,void*o){if(d!=s.device||!available()){errno=EACCES;return -1;}return s.inner->recv(d,p,n,ms,o);}
static R124Iface iface={.name="ipod_transport",.init=init,.link=link,.type=type,.caps=caps,.audio=audio,.send=tx,.recv=rx};
R125Module ipod_module={.name="transport_au57xbt",.version=0x200,.interfaces=&iface};
static int valid_offer(R123Transport*t,const R125Owner*o){
 return t&&t->opened&&t->active&&!t->failed&&o&&o->owner_context&&o->authorized&&o->start_driver&&o->input_ready&&o->stop_driver;
}
int r125_arm(R123Transport*t,const R125Owner*o){
 if(s.state!=R125_EMPTY||!valid_offer(t,o))return -1;
 s.transport=t;s.owner=*o;s.state=R125_ARMED;return 0;
}
int r127_bind_waiting(R123Transport*t,const R125Owner*o){
 /* All calls, including callbacks, MUST be serialized by the actual native
  * owner. This function is not an authorization provider. */
 if(s.state!=R127_WAITING||s.dispatching||!valid_offer(t,o)||!s.device||iface.owner!=s.device)return -1;
 if(o->owner_context(o->context,s.device)!=1||o->authorized(o->context,s.device,t->generation)!=1)return -1;
 s.transport=t;s.owner=*o;s.state=R125_ARMED;
 return init(&iface,"/dev/ipod","")? -1:0;
}
int r125_state(void){return s.state;}
int r125_stop(void){
 if(s.dispatching||s.state==R125_EMPTY||s.state==R125_RETIRED||s.state==R125_DIRTY)return -1;
 revoke();
 /* No vendor call on a lost owner context. Keep storage resident, retry only
  * on a verified owner context. Never unload or reuse the module here. */
 if(s.entered){if(!context_ok())return -1;s.dispatching=1;int rc=s.owner.stop_driver(s.owner.context,s.device);s.dispatching=0;if(rc){s.state=R125_DIRTY;return -2;}}
 s.state=R125_RETIRED;return 0;
}
int r125_dispatch(void){
 if(s.dispatching)return -1;
 if(s.state==R125_STOP_REQUIRED)return r125_stop();
 if(s.state!=R125_BOUND&&s.state!=R125_RUNNING)return -1;
 if(!authorized()){revoke();return -1;}
 s.dispatching=1;int rc;
 if(s.state==R125_BOUND){s.state=R125_STARTING;s.entered=1;rc=s.owner.start_driver(s.owner.context,s.device);if(rc||s.state!=R125_STARTING||!authorized()){revoke();s.dispatching=0;return -1;}s.state=R125_RUNNING;}
 rc=r124_driver_poll(s.owner.input_ready,s.owner.context);
 if(rc<0)revoke();
 s.dispatching=0;return rc;
}
