/* Isolated original parser export preflight. Never invoke guessed C++ ABI. */
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdint.h>
static int probe(void){
 const char *names[]={"_ZN4util6config10JSONParserC1Ev","_ZN4util6config10JSONParser16parseFileToArrayEPKcPPNS0_5ArrayEj","_ZN4util6config10JSONParserD1Ev","_ZN4util6config8ProviderC1EPKcS3_jNS1_12LoadStrategyE"};
 if(!dlopen("/proc/boot/liblab-slogf.so",RTLD_NOW|RTLD_GLOBAL)){puts("STEP8_SDK_PROVIDER_BLOCKED");return 2;}
 void *h=dlopen("/proc/boot/libutil.so",RTLD_NOW|RTLD_LOCAL);
 if(!h){printf("STEP8_LIBUTIL_LOAD_BLOCKED %s\n",dlerror());return 3;}
 for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++){
  if(!dlsym(h,names[i])){printf("STEP8_EXPORT_MISSING %u\n",i);return 4;}
  printf("STEP8_NATIVE_EXPORT_OK %u\n",i);
 }
 int fd=open("/etc/eso/production/connectivity.json",O_RDONLY);
 if(fd<0){perror("STEP8 fixture open");return 5;}
 uint32_t hash=2166136261u;unsigned total=0;unsigned char buf[1024];ssize_t n;
 while((n=read(fd,buf,sizeof buf))>0){for(ssize_t i=0;i<n;i++)hash=(hash^buf[i])*16777619u;total+=(unsigned)n;}
 close(fd);
 if(n<0||!total)return 6;
 printf("STEP8_FIXTURE_READ size=%u fnv=%08x\n",total,(unsigned)hash);
 puts("R105166_NATIVE_EXPORT_AND_FIXTURE_READ_NOT_PARSE");
 return 0;
}
int main(void){setvbuf(stdout,NULL,_IONBF,0);pid_t p=fork();if(p<0)return 8;if(!p)_exit(probe());int s=0;if(waitpid(p,&s,0)!=p||!WIFEXITED(s))return 9;return WEXITSTATUS(s);}
