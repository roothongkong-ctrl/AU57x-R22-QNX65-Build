/* Genuine mm-ipod + plugin in disposable QNX. No fake authentication. */
#if !defined(__QNXNTO__) || !defined(__arm__)
#error QNX ARM required
#endif
#include <process.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdlib.h>
#include "profile.h"
static int exact(const char *path){
 unsigned char b[4096];uint32_t h=2166136261u;unsigned long size=0;
 int fd=open(path,O_RDONLY);if(fd<0)return 0;ssize_t n;
 while((n=read(fd,b,sizeof b))>0){size+=(unsigned long)n;for(ssize_t i=0;i<n;i++)h=(h^b[i])*16777619u;}
 close(fd);return n==0&&size==PROGRAM_SIZE&&h==PROGRAM_FNV;
}
/* Private guest diagnostics; publish only classified tokens on the host. */
static int probe(const char *path,const char *label,int flags){
 (void)dlerror();
 void *h=dlopen(path,flags|RTLD_LOCAL);
 if(h){printf("IAP2_LOADER_%s_OK\n",label);dlclose(h);return 1;}
 const char *e=dlerror();
 printf("IAP2_LOADER_%s_BLOCKED\n",label);
 printf("PRIVATE_LINKER_DETAIL %s %s\n",label,e?e:"(no dlerror)");
 return 0;
}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);alarm(35);
 if(!exact("/proc/boot/mm-ipod")){puts("IAP2_ORIGINAL_BYTES_MISMATCH");return 2;}
 if(access("/dev/ipod0",F_OK)==0){puts("IAP2_BASELINE_NODE_ALREADY_PRESENT");return 3;}
 puts("IAP2_ORIGINAL_BYTES_OK");
 int driver=probe("/proc/boot/ipod-drvr-iap2.so","DRIVER_NOW",RTLD_NOW);
 puts(driver?"IAP2_ORIGINAL_PLUGIN_DLOPEN_OK":"IAP2_ORIGINAL_PLUGIN_DLOPEN_BLOCKED");
 int usb=probe("/proc/boot/ipod-transport-usbdevice.so","USB_NOW",RTLD_NOW);
 puts(usb?"IAP2_ORIGINAL_USB_TRANSPORT_DLOPEN_OK":"IAP2_ORIGINAL_USB_TRANSPORT_DLOPEN_BLOCKED");
 /* Lazy success is not service readiness; never call module symbols. */
 if(!driver)(void)probe("/proc/boot/ipod-drvr-iap2.so","DRIVER_LAZY",RTLD_LAZY);
 char *args[]={"mm-ipod","-d","iap2,config=/etc/mm/iap2.cfg,probe,ppsdir=/ramdisk/pps/iap2","-a","i2c,addr=0x11,path=/dev/i2c8,speed=50000","-t","usbdevice,path=/dev/io-usb/otg_dev","-l","/dev/shmem/iap2.log","-vvvvvv",NULL};
 /* Real recovered volume-control provider, only in disposable guest. */
 if(setenv("LD_PRELOAD","/proc/boot/liblab-hid-provider.so",1)!=0){puts("IAP2_PRELOAD_ENV_BLOCKED");return 5;}
 puts("IAP2_REAL_HOST_PRELOAD_REQUESTED");
 int pid=spawnv(P_NOWAIT,"/proc/boot/mm-ipod",args);
 if(pid<0){printf("IAP2_SPAWN_BLOCKED errno=%d\n",errno);puts("IAP2_DIAGNOSTIC_COMPLETE_NO_PHONE");return 0;}
 int reaped=0,seen=0,status=0;
 for(unsigned i=0;i<12;i++){
  if(!reaped){
   pid_t r=waitpid(pid,&status,WNOHANG);
   if(r==pid){reaped=1;printf("IAP2_CHILD_EXIT code=%d signal=%d\n",WIFEXITED(status)?WEXITSTATUS(status):-1,WIFSIGNALED(status)?WTERMSIG(status):0);}
   else if(r<0){reaped=1;printf("IAP2_WAIT_ERROR errno=%d\n",errno);}
  }
  if(access("/dev/ipod0",F_OK)==0&&!seen){seen=1;puts("IAP2_DEVICE_NODE_OBSERVED_NOT_AUTHENTICATED");}
  sleep(1);
 }
 if(!seen)puts("IAP2_DEVICE_NODE_NOT_OBSERVED");
 if(!reaped){kill(pid,SIGTERM);sleep(1);if(waitpid(pid,&status,WNOHANG)==0){kill(pid,SIGKILL);waitpid(pid,&status,0);}}
 puts("IAP2_DIAGNOSTIC_COMPLETE_NO_PHONE");alarm(0);return 0;
}
