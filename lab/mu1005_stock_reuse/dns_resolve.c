#include "dns_resolve.h"
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
typedef void *ref_t;
typedef void (*reply_t)(ref_t,uint32_t,uint32_t,int32_t,const char*,const char*,uint16_t,uint16_t,const unsigned char*,void*);
struct mu_dns_witness {
 ref_t ref; int (*live)(void*); void *ctx;
 int (*fd)(ref_t); int32_t (*process)(ref_t); void (*dispose)(ref_t);
 uint32_t index; uint16_t port; char fullname[1006],host[256]; int result;
};
static int sym(void*h,const char*n,void*out,size_t sz){
 void*p; if(sz!=sizeof p)return -1; (void)dlerror();p=dlsym(h,n);
 if(!p||dlerror())return -1;
 memcpy(out,&p,sz);return 0;
}
static int copy(char*out,size_t cap,const char*s){
 if(!s||!*s||strlen(s)>=cap)return -1;
 strcpy(out,s);return 0;
}
static void reply(ref_t ref,uint32_t flags,uint32_t index,int32_t error,
 const char*name,const char*host,uint16_t port,uint16_t len,const unsigned char*txt,void*v){
 mu_dns_witness*w=v;(void)flags;(void)len;(void)txt;
 if(w->result<0)return;
 if(ref!=w->ref||error||w->live(w->ctx)!=1){w->result=-1;return;}
 if(index==w->index&&name&&host&&!strcmp(name,w->fullname)&&
    !strcmp(host,w->host)&&ntohs(port)==w->port)w->result=1;
 else w->result=-1;
}
mu_dns_witness *mu_dns_begin(void*h,int(*live)(void*),void*ctx,uint32_t index,
 const char*instance,const char*type,const char*domain,const char*fullname,
 const char*host,uint16_t port){
 int32_t (*resolve)(ref_t*,uint32_t,uint32_t,const char*,const char*,const char*,reply_t,void*)=0;
 if(!h||!live||live(ctx)!=1||!index||index>=0xffffff00u||!port||
    !instance||!*instance||!type||!*type||!domain||!*domain)return NULL;
 mu_dns_witness*w=calloc(1,sizeof *w);if(!w)return NULL;
 w->live=live;w->ctx=ctx;w->index=index;w->port=port;
 if(copy(w->fullname,sizeof w->fullname,fullname)||copy(w->host,sizeof w->host,host)||
    sym(h,"DNSServiceResolve",&resolve,sizeof resolve)||
    sym(h,"DNSServiceRefSockFD",&w->fd,sizeof w->fd)||
    sym(h,"DNSServiceProcessResult",&w->process,sizeof w->process)||
    sym(h,"DNSServiceRefDeallocate",&w->dispose,sizeof w->dispose))goto bad;
 if(resolve(&w->ref,0,index,instance,type,domain,reply,w)!=0||!w->ref||live(ctx)!=1)goto bad;
 return w;
bad:mu_dns_end(w);return NULL;
}
int mu_dns_poll(mu_dns_witness*w){
 if(!w||w->live(w->ctx)!=1)return -1;
 if(w->result)return w->result;
 int fd=w->fd(w->ref);
 if(fd<0||fd>=FD_SETSIZE)return -1;
 fd_set set;FD_ZERO(&set);FD_SET(fd,&set);struct timeval tv={0,0};
 int n=select(fd+1,&set,NULL,NULL,&tv);
 if(n<0)return errno==EINTR?0:-1;
 if(!n)return 0;
 if(w->live(w->ctx)!=1||w->process(w->ref)!=0||w->live(w->ctx)!=1){w->result=-1;return -1;}
 return w->result;
}
void mu_dns_end(mu_dns_witness*w){
 if(!w)return;
 if(w->ref&&w->dispose)w->dispose(w->ref);
 free(w);
}
