#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#ifdef __QNXNTO__
#define _QNX_SOURCE 1
#endif
#include <pthread.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#ifdef __QNXNTO__
#define LOG_PATH "/dev/shmem/au57x_r143_btstack_probe.log"
#else
#define LOG_PATH "/tmp/au57x_r143_btstack_probe.log"
#endif
typedef int(*spawn_fn)(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*);
static void *worker(void *p){const char *name=p;int rc=pthread_setname_np(pthread_self(),name);return (void*)(long)rc;}
static int check_log(void){int fd=open(LOG_PATH,O_RDONLY);char b[8192];ssize_t n;if(fd<0)return 1;n=read(fd,b,sizeof b-1);close(fd);if(n<=0)return 2;b[n]=0;
 if(!strstr(b,"event=stock_image_pinned"))return 3;
 if(!strstr(b,"name=Thread_bluetoothProc")||!strstr(b,"name=bluesdk_mv8787bt_read")||!strstr(b,"name=bluesdk_mv8787bt_write"))return 4;
 if(!strstr(b,"role=primary_bluetoothProc_candidate")||!strstr(b,"role=secondary_bluesdk_read")||!strstr(b,"role=secondary_bluesdk_write"))return 5;
 if(!strstr(b,"target_observed=1")||!strstr(b,"caller_observed=1"))return 6;
 if(!strstr(b,"role=untrusted_callsite")||!strstr(b,"stock_pc=0"))return 7;
 puts("R143_QNX_PROCESS_SHA_PIN_PASS");puts("R143_QNX_PTHREAD_CREATE_TRANSPARENT_FORWARD_PASS");puts("R143_QNX_STOCK_ORIGIN_TRACKER_PASS_SYNTHETIC_CALLSITE");puts("R143_QNX_THREAD_NAME_TO_OWNER_CORRELATION_PASS");puts("R143_QNX_NO_OEM_API_NO_IAP2_NO_MFI_NO_INSTALL");return 0;}
int main(void){
 if(getenv("R143_NEGATIVE")){pthread_t t;void *v=0;if(pthread_create(&t,0,worker,"negative")!=0)return 20;if(pthread_join(t,&v)!=0||(long)v!=0)return 21;puts("R143_NEGATIVE_PROCESS_CONTINUES");return 0;}
 void *h=dlopen(NULL,RTLD_NOW);if(!h)return 10;void *sym=dlsym(h,"r143_test_create_stock_thread");spawn_fn spawn=0;if(!sym||sizeof sym!=sizeof spawn)return 11;memcpy(&spawn,&sym,sizeof spawn);
 const char *names[]={"Thread_bluetoothProc","bluesdk_mv8787bt_read","bluesdk_mv8787bt_write"};pthread_t t[3];void *v=0;
 for(int i=0;i<3;i++){int src=spawn(&t[i],0,worker,(void*)names[i]);if(src!=0){int(*active)(void)=0;int(*status)(void)=0;void *ap=dlsym(RTLD_DEFAULT,"au57x_r143_probe_active");void *sp=dlsym(RTLD_DEFAULT,"au57x_r143_probe_status");if(ap&&sizeof active==sizeof ap)memcpy(&active,&ap,sizeof active);if(sp&&sizeof status==sizeof sp)memcpy(&status,&sp,sizeof status);printf("R143_SPAWN_FAIL index=%d rc=%d probe_active=%d probe_status=%d\\n",i,src,active?active():-1,status?status():-1);return 12+i;}}
 for(int i=0;i<3;i++){v=0;if(pthread_join(t[i],&v)!=0)return 16+i;
#ifdef __linux__
 if((long)v!=0&&(long)v!=ERANGE)return 16+i;
#else
 if((long)v!=0)return 16+i;
#endif
 }
 pthread_t ordinary;if(pthread_create(&ordinary,0,worker,"ordinary_worker")!=0)return 19;if(pthread_join(ordinary,&v)!=0||(long)v!=0)return 20;
 int rc=check_log();if(rc){printf("R143_QNX_FAIL_LOG=%d\n",rc);return 30+rc;}return 0;
}
