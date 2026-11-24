#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
/* Read-only review: no hooks, registration, SDK calls or OEM writes. */
static pid_t constructor_pid;
static unsigned constructor_count;
__attribute__((constructor)) static void loaded(void){constructor_pid=getpid();++constructor_count;}
int r137_preload_pid(void){return (int)constructor_pid;}
unsigned r137_preload_count(void){return constructor_count;}
int r137_preload_readself(void){
#if defined(__QNXNTO__) && defined(__arm__)
 int fd=open("/proc/self/as",O_RDONLY);if(fd<0)return -1;
 unsigned observed=0;ssize_t n;
 do{n=pread(fd,&observed,sizeof observed,(off_t)(uintptr_t)&constructor_count);}while(n<0&&errno==EINTR);
 close(fd);return n==(ssize_t)sizeof observed&&observed==1?1:0;
#else
 return -2;
#endif
}
