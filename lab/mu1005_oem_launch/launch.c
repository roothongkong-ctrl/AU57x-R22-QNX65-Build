/* QNX ARM original process launch. Lab-only; never on vehicle. */
#if !defined(__QNXNTO__) || !defined(__arm__)
#error QNX ARM required
#endif
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <process.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "profile.h"
static resmgr_connect_funcs_t conn;
static resmgr_io_funcs_t io;
static iofunc_attr_t attr;
static int observe_open(resmgr_context_t *ctp,io_open_t *msg,RESMGR_HANDLE_T *handle,void *extra){
 int r=iofunc_open_default(ctp,msg,handle,extra);
 printf("SDIO_OPEN result=%d\n",r);return r;
}
static int reject_devctl(resmgr_context_t *ctp,io_devctl_t *msg,RESMGR_OCB_T *ocb){
 (void)ctp;(void)ocb;printf("SDIO_DEVCTL_REJECT code=%u\n",(unsigned)msg->i.dcmd);return ENOSYS;
}
static int reject_msg(resmgr_context_t *ctp,io_msg_t *msg,RESMGR_OCB_T *ocb){
 (void)ctp;(void)msg;(void)ocb;puts("SDIO_MSG_REJECT");return ENOSYS;
}
static void *serve(void *arg){
 dispatch_context_t *c=arg;
 while((c=dispatch_block(c))!=NULL)dispatch_handler(c);
 return NULL;
}
static void model(void){
 dispatch_t *d=dispatch_create();resmgr_attr_t ra;pthread_t th;
 if(!d){puts("SDIO_MODEL_UNAVAILABLE");return;}
 memset(&ra,0,sizeof ra);ra.nparts_max=2;ra.msg_max_size=2048;
 iofunc_func_init(_RESMGR_CONNECT_NFUNCS,&conn,_RESMGR_IO_NFUNCS,&io);
 conn.open=observe_open;io.devctl=reject_devctl;io.msg=reject_msg;
 iofunc_attr_init(&attr,S_IFNAM|0600,NULL,NULL);
 if(resmgr_attach(d,&ra,"/dev/sdio0",_FTYPE_ANY,0,&conn,&io,&attr)<0){puts("SDIO_MODEL_ATTACH_FAIL");return;}
 dispatch_context_t *c=dispatch_context_alloc(d);
 if(!c||pthread_create(&th,NULL,serve,c)){puts("SDIO_MODEL_THREAD_FAIL");return;}
 int fd=open("/dev/sdio0",O_RDWR);
 if(fd<0){printf("SDIO_MODEL_BASELINE_FAIL errno=%d\n",errno);return;}
 close(fd);puts("SDIO_MODEL_BASELINE_OPEN_OK");
 /* No successful hardware control, HCI data, RF or MFi is emulated. */
}
static int check_bytes(const char *path){
 unsigned char b[4096];uint32_t h=2166136261u;unsigned long size=0;
 int fd=open(path,O_RDONLY);if(fd<0)return 0;
 ssize_t n;
 while((n=read(fd,b,sizeof b))>0){size+=(unsigned long)n;for(ssize_t i=0;i<n;i++)h=(h^b[i])*16777619u;}
 close(fd);return n==0&&size==PROGRAM_SIZE&&h==PROGRAM_FNV;
}
int main(int argc,char **argv){
 setvbuf(stdout,NULL,_IONBF,0);alarm(35);
 const char *path="/proc/boot/" PROGRAM;
 if(!check_bytes(path)){puts("ORIGINAL_GUEST_BYTES_MISMATCH");return 2;}
 printf("ORIGINAL_GUEST_BYTES_OK %s\n",PROGRAM);
 if(argc==2&&!strcmp(argv[1],"model")&&!strcmp(PROGRAM,"btstack"))model();
 char *bt[]={PROGRAM,"btstack","1",NULL};
 char *blue[]={PROGRAM,"bluetooth","4",NULL};
 char *si[]={PROGRAM,"-sm-name","smartphone_integrator","-sm-id","353","-sm-time","3051","-sm-restart","0",NULL};
 char *ipod[]={PROGRAM,"-d","iap2,config=/etc/mm/iap2.cfg,probe,ppsdir=/ramdisk/pps/iap2","-a","i2c,addr=0x11,path=/dev/i2c8,speed=50000","-t","usbdevice,path=/dev/io-usb/otg_dev","-l","/dev/shmem/iap2.log","-vvvvvv",NULL};
 char *dio[]={PROGRAM,NULL};
 char *uap[]={PROGRAM,"-i","uap0","sys_cfg_bss_status",NULL};
 char **args=!strcmp(PROGRAM,"btstack")?bt:!strcmp(PROGRAM,"bluetooth")?blue:!strcmp(PROGRAM,"smartphone_integrator")?si:!strcmp(PROGRAM,"mm-ipod")?ipod:!strcmp(PROGRAM,"uaputl")?uap:dio;
 /* spawnv avoids fork in a multithreaded QNX resource-manager process. */
 pid_t pid=spawnv(P_NOWAIT,path,args);
 if(pid<0){printf("ORIGINAL_SPAWN_FAIL errno=%d\n",errno);puts("ORIGINAL_LAUNCH_DIAGNOSTIC_COMPLETE_NOT_CONNECTION");alarm(0);return 0;}
 printf("ORIGINAL_EXEC_ATTEMPT %s\n",PROGRAM);
 int status=0;
 for(unsigned i=0;i<8;i++){
  pid_t r=waitpid(pid,&status,WNOHANG);
  if(r==pid){printf("ORIGINAL_CHILD_EXIT status=%d signal=%d\n",WIFEXITED(status)?WEXITSTATUS(status):-1,WIFSIGNALED(status)?WTERMSIG(status):0);goto done;}
  if(r<0){printf("ORIGINAL_WAIT_FAIL errno=%d\n",errno);return 4;}
  sleep(1);
 }
 puts("ORIGINAL_CHILD_ALIVE_8S_NOT_SERVICE_READY");
 kill(pid,SIGTERM);
 sleep(1);
 if(waitpid(pid,&status,WNOHANG)==0){kill(pid,SIGKILL);waitpid(pid,&status,0);}
done:
 puts("ORIGINAL_LAUNCH_DIAGNOSTIC_COMPLETE_NOT_CONNECTION");
 alarm(0);return 0;
}
