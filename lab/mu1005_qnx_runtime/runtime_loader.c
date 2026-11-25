/* QNX 6.5 guest proof: dlopen the EXACT prebuilt native library, not a
 * mock. No original address, attach, SDK, Bluetooth or vehicle I/O. */
#if !defined(__QNXNTO__) || !defined(__arm__)
#error QNX ARM target required
#endif
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "native_blob.h"
static int copy_all(int fd){
 size_t off=0;
 while(off<sizeof native_blob){
  ssize_t n=write(fd,native_blob+off,sizeof native_blob-off);
  if(n<0&&errno==EINTR)continue;
  if(n<=0)return -1;
  off+=(size_t)n;
 }
 return 0;
}
static int compare_all(int fd){
 unsigned char bytes[1024];size_t off=0;
 if(lseek(fd,0,SEEK_SET)!=0)return -1;
 while(off<sizeof native_blob){
  ssize_t n=read(fd,bytes,sizeof bytes);
  if(n<0&&errno==EINTR)continue;
  if(n<=0||(size_t)n>sizeof native_blob-off||memcmp(bytes,native_blob+off,(size_t)n))return -1;
  off+=(size_t)n;
 }
 return read(fd,bytes,1)==0?0:-1;
}
int main(void){
 struct utsname u;char path[]="/dev/shmem/mu1005-bt-native-review.so";
 static const char *const symbols[]={"mu_bt_native_attach","mu_bt_native_tick","mu_bt_native_withdraw","inbound_publish","ir5_publish"};
 int fd=-1,created=0,rc=1;void *handle=NULL;
 if(uname(&u)||strcmp(u.sysname,"QNX")||strncmp(u.release,"6.5",3))goto done;
 printf("QNX_KERNEL=%s %s %s\n",u.sysname,u.release,u.machine);
 fd=open(path,O_CREAT|O_EXCL|O_RDWR,0700);if(fd<0)goto done;created=1;
 if(copy_all(fd)||compare_all(fd))goto done;
 if(close(fd)){fd=-1;goto done;}fd=-1;
 handle=dlopen(path,RTLD_NOW|RTLD_LOCAL);
 if(!handle){fprintf(stderr,"dlopen failed: %s\n",dlerror());goto done;}
 for(unsigned i=0;i<sizeof symbols/sizeof symbols[0];++i){
  if(!dlsym(handle,symbols[i])){fprintf(stderr,"symbol missing: %s\n",symbols[i]);goto done;}
  printf("DLSYM_OK:%s\n",symbols[i]);
 }
 /* Deliberately do NOT call any symbol from the native library. */
 if(dlclose(handle)){handle=NULL;goto done;}handle=NULL;
 rc=0;
done:
 if(handle)(void)dlclose(handle);
 if(fd>=0)(void)close(fd);
 if(created&&unlink(path))rc=1;
 if(rc==0)puts("MU1005_QNX65_NATIVE_DLOPEN_OK_NO_OEM_ATTACH");
 return rc;
}
