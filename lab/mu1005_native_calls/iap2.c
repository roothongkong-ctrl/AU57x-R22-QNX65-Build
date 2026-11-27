/* Exact OEM iAP2 client, negative-only QNX IPC observation. NO INSTALL.
 * The endpoint is deliberately private and rejects every control message.
 * No production device, successful connect reply, MFi, or RF is emulated.
 */
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>

static const char path[]="/dev/au57x-r105115-reject";
static const char missing[]="/dev/au57x-r105115-absent";
static resmgr_connect_funcs_t conn;
static resmgr_io_funcs_t io;
static iofunc_attr_t attr;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static unsigned received, valid;
static int reject_msg(resmgr_context_t *ctp,io_msg_t *msg,RESMGR_OCB_T *ocb) {
    struct _io_msg hdr;
    (void)msg;(void)ocb;
    memset(&hdr,0,sizeof hdr);
    int n=MsgRead(ctp->rcvid,&hdr,sizeof hdr,0);
    pthread_mutex_lock(&lock);
    received++;
    if(n==(int)sizeof hdr && hdr.type==_IO_MSG){
        valid++;
        printf("OEM_IAP2_IPC_HEADER type=0x%04x combine_len=0x%04x mgrid=0x%04x subtype=0x%04x\n",
          (unsigned)hdr.type,(unsigned)hdr.combine_len,(unsigned)hdr.mgrid,(unsigned)hdr.subtype);
    }else printf("OEM_IAP2_IPC_HEADER_UNAVAILABLE bytes=%d\n",n);
    puts("OEM_IAP2_IPC_REJECT ENOSYS");
    fflush(stdout);
    pthread_mutex_unlock(&lock);
    return ENOSYS;
}
static void *serve(void *arg) {
    dispatch_context_t *ctp=arg;
    while((ctp=dispatch_block(ctp))!=NULL) {
        dispatch_handler(ctp);
    }
    puts("LAB_DISPATCH_STOPPED");
    return NULL;
}
typedef void *(*connect_fn)(const char *,void *);
typedef void (*disconnect_fn)(void *);
int main(void){
    alarm(25);
    setvbuf(stdout,NULL,_IONBF,0);
    void *h=dlopen("/proc/boot/libiap2client.so.1",RTLD_NOW|RTLD_LOCAL);
    if(!h){const char *e=dlerror();printf("OEM_IAP2_LOAD_FAIL %s\n",e?e:"unknown");return 2;}
    void *p=dlsym(h,"iap2_connect"),*q=dlsym(h,"iap2_disconnect");
    connect_fn connect_oem=NULL;disconnect_fn disconnect_oem=NULL;
    if(!p||!q||sizeof p!=sizeof connect_oem||sizeof q!=sizeof disconnect_oem)return 3;
    memcpy(&connect_oem,&p,sizeof p);memcpy(&disconnect_oem,&q,sizeof q);
    errno=0;
    if(access(missing,F_OK)==0 || errno!=ENOENT){puts("LAB_MISSING_PATH_NOT_SAFE");return 4;}
    errno=0;
    void *client=connect_oem(missing,NULL);
    int saved=errno;
    if(client){disconnect_oem(client);puts("OEM_IAP2_MISSING_PATH_UNEXPECTED_HANDLE");return 5;}
    printf("OEM_IAP2_MISSING_PATH_REJECTED errno=%d\n",saved);
    dispatch_t *dpp=dispatch_create();
    if(!dpp){perror("dispatch_create");return 6;}
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS,&conn,_RESMGR_IO_NFUNCS,&io);
    io.msg=reject_msg;
    iofunc_attr_init(&attr,S_IFNAM|0600,NULL,NULL);
    resmgr_attr_t ra;
    memset(&ra,0,sizeof ra);ra.nparts_max=2;ra.msg_max_size=2048;
    if(resmgr_attach(dpp,&ra,path,_FTYPE_ANY,0,&conn,&io,&attr)==-1){perror("resmgr_attach");return 7;}
    dispatch_context_t *ctp=dispatch_context_alloc(dpp);
    if(!ctp)return 8;
    pthread_t th;
    if(pthread_create(&th,NULL,serve,ctp)!=0){puts("LAB_THREAD_FAIL");return 9;}
    /* Independently prove the private endpoint can accept an open and
       dispatch a QNX _IO_MSG before attributing failures to the OEM call. */
    int fd=open(path,O_RDWR);
    if(fd<0){printf("LAB_BASELINE_OPEN_FAIL errno=%d\n",errno);return 13;}
    puts("LAB_BASELINE_OPEN_OK");
    struct _io_msg baseline;
    memset(&baseline,0,sizeof baseline);
    baseline.type=_IO_MSG;
    baseline.combine_len=sizeof baseline;
    baseline.mgrid=0x7f;
    baseline.subtype=1;
    errno=0;
    int mr=MsgSend(fd,&baseline,sizeof baseline,NULL,0);
    int me=errno;
    close(fd);
    pthread_mutex_lock(&lock);
    unsigned baseline_count=received,baseline_headers=valid;
    received=valid=0;
    pthread_mutex_unlock(&lock);
    printf("LAB_BASELINE_MSG result=%d errno=%d messages=%u valid=%u\n",mr,me,baseline_count,baseline_headers);
    if(mr!=-1||me!=ENOSYS||!baseline_count||!baseline_headers)return 14;
    puts("LAB_BASELINE_IPC_OK");
    errno=0;
    client=connect_oem(path,NULL);
    saved=errno;
    if(client){disconnect_oem(client);puts("OEM_IAP2_REJECT_UNEXPECTED_HANDLE");return 10;}
    pthread_mutex_lock(&lock);
    unsigned count=received,headers=valid;
    pthread_mutex_unlock(&lock);
    printf("OEM_IAP2_REJECT_RESULT errno=%d messages=%u valid_headers=%u\n",saved,count,headers);
    if(!count||!headers){puts("OEM_IAP2_IPC_NOT_OBSERVED");return 11;}
    if(dlclose(h)!=0)return 12;
    puts("OEM_IAP2_REAL_CALL_NEGATIVE_IPC_PASS");
    puts("SCOPE=REAL_CLIENT_REJECT_ONLY_NO_OEM_DRIVER_NO_PHONE_NO_MFI_NO_RF");
    /* Disposable QNX process: exiting removes the private resource manager. */
    alarm(0);
    return 0;
}
