/* R143: one SHA-pinned, read-only observer combining R92 process entry and
 * R97 stock pthread origin tracking with R142 thread-name classification.
 * It never calls BlueSDK, registers RFCOMM, changes GOT, or claims iAP2/MFi. */
#define _POSIX_C_SOURCE 200809L
#include "bt_thread_owner.h"
#include "sha256.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef AU57X_R143_STOCK_SHA256
#define AU57X_R143_STOCK_SHA256 "51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
#endif
#ifndef AU57X_R143_LOG_PATH
#define AU57X_R143_LOG_PATH "/tmp/au57x_r143_btstack_probe.log"
#endif
#if defined(AU57X_R143_QNX_TEST)
#define AU57X_R143_STOCK_PATH "/proc/boot/btstack"
#elif defined(AU57X_R143_HOST_TEST)
#define AU57X_R143_STOCK_PATH_ENV "AU57X_R143_HOST_TARGET"
#else
#define AU57X_R143_STOCK_PATH "/eso/bin/apps/btstack"
#endif
extern char *__progname;
static volatile unsigned probe_enabled;
static volatile int probe_status;
static const uintptr_t stock_callers[4]={0x236668u,0x2368c8u,0x236c2cu,0x236c48u};
typedef int(*create_fn)(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*);
typedef int(*setname_fn)(pthread_t,const char*);
static create_fn real_create;
static setname_fn real_setname;
static void *libc_handle;
static char log_path[256]=AU57X_R143_LOG_PATH;
static int digest_hex(const char*path,char out[65]){unsigned char b[8192],d[32];static const char hex[]="0123456789abcdef";sha256_ctx c;size_t n,i;FILE*f;if(!path||(f=fopen(path,"rb"))==NULL)return 0;sha256_init(&c);while((n=fread(b,1,sizeof b,f))!=0)sha256_update(&c,b,n);if(ferror(f)){fclose(f);return 0;}if(fclose(f))return 0;sha256_final(&c,d);for(i=0;i<32;i++){out[2*i]=hex[d[i]>>4];out[2*i+1]=hex[d[i]&15];}out[64]=0;return 1;}
static size_t add(char*d,size_t cap,size_t n,const char*s){while(s&&*s&&n+1<cap)d[n++]=*s++;if(cap)d[n<cap?n:cap-1]=0;return n;}
static size_t add_tid(char*d,size_t cap,size_t n,pthread_t t){static const char h[]="0123456789abcdef";const unsigned char*p=(const unsigned char*)&t;size_t i;for(i=0;i<sizeof t&&n+2<cap;i++){d[n++]=h[p[i]>>4];d[n++]=h[p[i]&15];}if(cap)d[n<cap?n:cap-1]=0;return n;}
static void log_line(const char*event,const char*role,pthread_t tid,const char*name,uintptr_t caller,uintptr_t start,int stock,int rc,int target,int current){char b[512];size_t n=0;int fd;char num[32];b[0]=0;n=add(b,sizeof b,n,"event=");n=add(b,sizeof b,n,event);if(role){n=add(b,sizeof b,n," role=");n=add(b,sizeof b,n,role);}n=add(b,sizeof b,n," pid=");snprintf(num,sizeof num,"%ld",(long)getpid());n=add(b,sizeof b,n,num);n=add(b,sizeof b,n," tid=");n=add_tid(b,sizeof b,n,tid);if(name){n=add(b,sizeof b,n," name=");for(size_t i=0;name[i]&&i<80&&n+2<sizeof b;i++){unsigned char c=(unsigned char)name[i];b[n++]=(c>=32&&c<=126)?(char)c:'?';}b[n]=0;}if(caller){n=add(b,sizeof b,n," caller=");snprintf(num,sizeof num,"0x%lx",(unsigned long)caller);n=add(b,sizeof b,n,num);}if(start){n=add(b,sizeof b,n," start=");snprintf(num,sizeof num,"0x%lx",(unsigned long)start);n=add(b,sizeof b,n,num);}if(stock>=0){n=add(b,sizeof b,n," stock_pc=");snprintf(num,sizeof num,"%d",stock);n=add(b,sizeof b,n,num);}if(rc>=0){n=add(b,sizeof b,n," rc=");snprintf(num,sizeof num,"%d",rc);n=add(b,sizeof b,n,num);}if(target>=0){n=add(b,sizeof b,n," target_observed=");snprintf(num,sizeof num,"%d",target);n=add(b,sizeof b,n,num);}if(current>=0){n=add(b,sizeof b,n," caller_observed=");snprintf(num,sizeof num,"%d",current);n=add(b,sizeof b,n,num);}n=add(b,sizeof b,n,"\n");fd=open(log_path,O_WRONLY|O_CREAT|O_APPEND,0600);if(fd>=0){ssize_t wr=write(fd,b,n);(void)wr;close(fd);}}
static void resolve_symbols(void){void*p=dlsym(RTLD_NEXT,"pthread_create");if(p&&sizeof real_create==sizeof p)memcpy(&real_create,&p,sizeof real_create);p=dlsym(RTLD_NEXT,"pthread_setname_np");if(!p){libc_handle=dlopen("libc.so.3",RTLD_NOW|RTLD_LOCAL);if(libc_handle)p=dlsym(libc_handle,"pthread_setname_np");}if(p&&sizeof real_setname==sizeof p)memcpy(&real_setname,&p,sizeof real_setname);}
__attribute__((constructor)) static void r143_init(void) {
    char digest[65];
    const char *opt = getenv("AU57X_R143_PROBE");
    const char *custom = getenv("AU57X_R143_LOG");
    const char *stock_path;
    resolve_symbols();
    if (custom && custom[0] == '/' && strlen(custom) < sizeof log_path)
        memcpy(log_path, custom, strlen(custom) + 1);
    if (!opt || strcmp(opt, "1") != 0) { probe_status = 1; return; }
    if (!__progname || strcmp(__progname, "btstack") != 0) {
        log_line("process_reject", "wrong_basename", pthread_self(), __progname,
                 0, 0, 0, -1, -1, -1);
        probe_status = 2;
        return;
    }
#if defined(AU57X_R143_HOST_TEST)
    stock_path = getenv(AU57X_R143_STOCK_PATH_ENV);
#else
    stock_path = AU57X_R143_STOCK_PATH;
#endif
    if (!digest_hex(stock_path, digest)) {
        char why[96]; snprintf(why, sizeof why, "stock_open_or_read_errno_%d", errno);
        log_line("process_reject", why, pthread_self(), NULL, 0, 0, 0, -1, -1, -1);
        probe_status = 31;
        return;
    }
    if (strcmp(digest, AU57X_R143_STOCK_SHA256) != 0) {
        log_line("process_reject", "stock_sha256_mismatch", pthread_self(), digest,
                 0, 0, 0, -1, -1, -1);
        probe_status = 32;
        return;
    }
    if (!real_create || !real_setname) {
        log_line("observer_reject", "pthread_symbol_missing", pthread_self(), NULL,
                 0, 0, 0, -1, -1, -1);
        probe_status = 4;
        return;
    }
    probe_enabled = 1;
    probe_status = 5;
    log_line("stock_image_pinned", "MU1005_btstack", pthread_self(), NULL,
             0, 0, 1, 0, -1, -1);
}
int pthread_create(pthread_t*t,const pthread_attr_t*a,void*(*start)(void*),void*arg){uintptr_t site;int stock=0,rc;if(!real_create)return ENOSYS;if(!probe_enabled)return real_create(t,a,start,arg);site=(uintptr_t)__builtin_return_address(0);for(size_t i=0;i<4;i++)if((site&~(uintptr_t)1)==(uintptr_t)stock_callers[i]+4u)stock=1;if(stock)rc=au97_pthread_create(t,a,start,arg,site,real_create);else rc=real_create(t,a,start,arg);log_line("pthread_create",stock?"MU1005_stock_callsite":"untrusted_callsite",rc==0?*t:pthread_self(),NULL,site,(uintptr_t)start,stock,rc,-1,-1);return rc;}
int pthread_setname_np(pthread_t thread,const char*name){int rc;if(!real_setname)return ENOSYS;rc=real_setname(thread,name);if(probe_enabled&&name){const char*role=NULL;if(name[0]&&strstr(name+1,"bluetoothProc"))role="primary_bluetoothProc_candidate";else if(strstr(name,"mv8787bt_read"))role="secondary_bluesdk_read";else if(strstr(name,"mv8787bt_write"))role="secondary_bluesdk_write";if(role)log_line("thread_name",role,thread,name,0,0,-1,rc,au97_is_observed_thread(thread),au97_current_observed_thread());}return rc;}
int au57x_r143_probe_active(void){return probe_enabled?1:0;}
int au57x_r143_probe_status(void){return probe_status;}
#ifdef AU57X_R143_TEST_API
int r143_test_create_stock_thread(pthread_t*t,const pthread_attr_t*a,void*(*start)(void*),void*arg){if(!real_create||!probe_enabled)return -1;return au97_pthread_create(t,a,start,arg,0x23666cu,real_create);}
#endif
