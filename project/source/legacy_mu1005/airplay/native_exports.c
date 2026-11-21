#include "native_exports.h"
#include <dlfcn.h>
#include <string.h>
#define UTF8 0x08000100u
static int symbol(void *h,const char *n,void *out,size_t len){
 void *p; if(len!=sizeof p)return -1; (void)dlerror();p=dlsym(h,n);
 if(!p||dlerror())return -1;
 memcpy(out,&p,len);return 0;
}
static int library(void *p){MuDioNative *n=p;return n->guard.verified_library(n->guard.opaque,n->handle)==1;}
static int owner(void *p){MuDioNative *n=p;return n->guard.owner_exclusive(n->guard.opaque,n->server,n->manager)==1 && (!n->server_slot || *n->server_slot==n->server) && (!n->get_queue || (n->queue && n->get_queue(n->server)==n->queue));}
static int valid(void *p,const void *s){MuDioNative *n=p;return s&&library(n)&&owner(n)&&n->get_type(s)==n->string_type();}
static int utf8(void *p,const void *s,char *out,size_t cap){
 MuDioNative *n=p;
 if(!valid(p,s)||!out||cap==0||cap>17||!n->get_string(s,out,(long)cap,UTF8))return -1;
 for(size_t i=0;i<cap;i++)if(!out[i])return (int)i;
 return -1;
}
static int setprop(void *p,void *server,const void *key,const void *value){
 MuDioNative *n=p;
 if(server!=n->server||key!=n->key||!valid(p,value))return -1;
 /* OEM 5 argument ABI: server, flags, property, qualifier, value. */
 n->interface_dirty=1;
 int rc=n->set(server,0,key,NULL,value);
 char name[17];
 if(rc!=0)return rc;
 if(!library(n)||!owner(n)||utf8(p,value,name,sizeof name)<=0||
    n->guard.interface_matches(n->guard.opaque,server,n->manager,name)!=1)return -1;
 return 0;
}
static int control(void *p,void *server,const void *cmd,const void *manager){
 MuDioNative *n=p;
 if(server!=n->server||manager!=n->manager||(cmd!=n->start&&cmd!=n->stop)||!valid(p,cmd))return -1;
 /* OEM DIO caller uses flag 1 and three NULL arguments. The CDIOManager
  * is delegate context, NOT a qualifier or a parameter dictionary. */
 return n->control(server,1,cmd,NULL,NULL,NULL);
}
static int listener(void *p,void *server,const void *manager,unsigned port){
 MuDioNative *n=p;
 if(server!=n->server||manager!=n->manager||!library(n)||!owner(n))return -1;
 char name[17];
 if(utf8(p,n->iface,name,sizeof name)<=0 ||
    n->guard.interface_matches(n->guard.opaque,server,manager,name)!=1)return -1;
 return n->guard.listener(n->guard.opaque,server,manager,port);
}
static int name_ok(const char *p){
 if(!p)return 0;
 size_t i=0;for(;i<17&&p[i];i++){unsigned char c=(unsigned char)p[i];if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;}
 return i>0&&i<17;
}
static void clear(MuDioNative *n){
 if(n->release){if(n->key)n->release(n->key);if(n->start)n->release(n->start);if(n->stop)n->release(n->stop);if(n->iface)n->release(n->iface);if(n->original)n->release(n->original);}
 memset(n,0,sizeof *n);
}
int mu_dio_bind_interface(MuDioNative *n,void *h,void *server,const void *manager,uint32_t gen,const MuDioGuard *g,const char *orig,const char *target){
 if(!n||n->initialized||!h||!server||!manager||!gen||!g||!g->verified_library||!g->owner_exclusive||!g->listener||!g->stopped||!g->interface_matches||!name_ok(orig)||!name_ok(target))return -1;
 memset(n,0,sizeof *n);n->handle=h;n->server=server;n->manager=manager;n->guard=*g;
 if(!library(n)||!owner(n)||g->stopped(g->opaque,server,manager)!=1)goto bad;
#define SYM(field,name) if(symbol(h,name,&n->field,sizeof n->field))goto bad
 SYM(get_type,"CFGetTypeID");SYM(string_type,"CFStringGetTypeID");SYM(server_type,"AirPlayReceiverServerGetTypeID");
 SYM(make_string,"CFStringCreateWithCString");SYM(get_string,"CFStringGetCString");SYM(release,"CFRelease");
 SYM(set,"AirPlayReceiverServerSetProperty");SYM(control,"AirPlayReceiverServerControl");
 SYM(server_slot,"gAirPlayReceiverServer");SYM(get_queue,"AirPlayReceiverServerGetDispatchQueue");
#undef SYM
 if(*n->server_slot!=server||n->get_type(server)!=n->server_type())goto bad;
 n->queue=n->get_queue(server);if(!n->queue||!owner(n)||g->interface_matches(g->opaque,server,manager,orig)!=1)goto bad;
 n->key=n->make_string(NULL,"interfaceName",UTF8);n->start=n->make_string(NULL,"startServer",UTF8);n->stop=n->make_string(NULL,"stopServer",UTF8);
 n->iface=n->make_string(NULL,target,UTF8);n->original=n->make_string(NULL,orig,UTF8);
 if(!n->key||!n->start||!n->stop||!n->iface||!n->original)goto bad;
 Mu34ApServerBinding b={.library_verified=library,.cf_string_ok=valid,.cf_string_utf8=utf8,.owner_thread_ok=owner,.set_property=setprop,.server_control=control,.listening_verified=listener,.opaque=n,.server=server,.owner=manager,.interface_key=n->key,.start_command=n->start,.stop_command=n->stop,.generation=gen};
 if(mu34_ap_server_init(&n->ap,&b))goto bad;
 n->initialized=1;return 0;
bad:clear(n);return -1;
}
int mu_dio_bind(MuDioNative *n,void *h,void *server,const void *manager,uint32_t gen,const MuDioGuard *g,const char *orig){
 return mu_dio_bind_interface(n,h,server,manager,gen,g,orig,"uap0");
}
int mu_dio_start(MuDioNative *n,unsigned port){
 if(!n||!n->initialized||!port||port>65535||!library(n)||!owner(n)||n->guard.stopped(n->guard.opaque,n->server,n->manager)!=1)return -1;
 int rc=mu34_ap_server_set_interface(&n->ap,n->ap.generation,n->iface);
 return rc==0?mu34_ap_server_start(&n->ap,n->ap.generation,port):rc;
}
int mu_dio_poll(MuDioNative *n){return n&&n->initialized?mu34_ap_server_poll(&n->ap,n->ap.generation):-1;}
int mu_dio_restore(MuDioNative *n){
 if(!n||!n->initialized||!library(n)||!owner(n))return -1;
 if(n->ap.start_requested&&mu34_ap_server_stop(&n->ap,n->ap.generation)!=0)return -1;
 /* Controller accepted stop != confirmed teardown. Retain all objects until barrier. */
 int stopped=n->guard.stopped(n->guard.opaque,n->server,n->manager);
 if(stopped!=1)return stopped==0?1:-1;
 if(n->interface_dirty&&setprop(n,n->server,n->key,n->original)!=0)return -1;
 if(mu34_ap_server_close(&n->ap,n->ap.generation)!=0)return -1;
 clear(n);return 0;
}
