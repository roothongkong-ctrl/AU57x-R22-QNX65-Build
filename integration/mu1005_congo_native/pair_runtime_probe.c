/* Native isolated runtime probe, no vehicle devices, no production code.
 * Do not install. Never configure fake MFi or preload authentication stubs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
int main(void){
    if(access("/dev/i2c8",F_OK)==0){
        puts("REFUSE_HARDWARE_PRESENT");return 9;
    }
    setvbuf(stdout,NULL,_IONBF,0);
    void *lib=dlopen("/proc/boot/libairplay.so",RTLD_NOW|RTLD_LOCAL);
    if(!lib){printf("DONOR_PAIR_LIBRARY_LOAD_FAILED: %s\n",dlerror());return 2;}
    puts("DONOR_PAIR_LIBRARY_LOAD_OK");
    const char *names[]={"PairingSessionCreate","PairingSessionExchange","PairingSessionSetDelegate","APSMFiPlatform_CopyCertificate","APSMFiPlatform_CreateSignature",NULL};
    for(int i=0;names[i];i++)printf("DONOR_EXPORT %s %s\n",names[i],dlsym(lib,names[i])?"present":"missing");
    /* Run only in this QEMU environment with no physical authentication device.
       The helper may fail safely; no claims of MFi or actual phone pairing. */
    char *argv[]={"/proc/boot/wcppaird",NULL};
    char *env[]={"PATH=/proc/boot","LD_LIBRARY_PATH=/proc/boot",
        "WCP_NVAIRPLAY=/proc/boot/libairplay.so","WCP_PAIRDIR=/dev/shmem",NULL};
    pid_t pid;int e=posix_spawn(&pid,argv[0],NULL,NULL,argv,env);
    if(e){printf("DONOR_HELPER_SPAWN_FAILED %d\n",e);return 3;}
    puts("DONOR_HELPER_SPAWN_OK");
    for(int i=0;i<30;i++){
        int status=0;pid_t r=waitpid(pid,&status,WNOHANG);
        if(r==pid){
            if(WIFEXITED(status))printf("DONOR_HELPER_EXIT %d\n",WEXITSTATUS(status));
            else if(WIFSIGNALED(status))printf("DONOR_HELPER_SIGNAL %d\n",WTERMSIG(status));
            return 4;
        }
        if(r<0&&errno!=EINTR){perror("waitpid");return 5;}
        struct timespec delay={0,100000000};nanosleep(&delay,NULL);
    }
    if(kill(pid,SIGTERM)&&errno!=ESRCH)return 6;
    for(int i=0;i<10;i++){
        int status=0;pid_t r=waitpid(pid,&status,WNOHANG);
        if(r==pid){puts("DONOR_HELPER_SURVIVED_3S_AND_STOPPED_NO_HARDWARE");return 0;}
        struct timespec delay={0,100000000};nanosleep(&delay,NULL);
    }
    (void)kill(pid,SIGKILL);(void)waitpid(pid,NULL,0);
    puts("DONOR_HELPER_SURVIVED_3S_KILLED_NO_HARDWARE");
    return 0;
}
