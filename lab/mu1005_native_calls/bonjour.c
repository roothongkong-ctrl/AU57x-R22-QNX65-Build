/* Call exact original Bonjour API with no RF or phone. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
typedef void *dns_ref;
typedef int32_t (*create_fn)(dns_ref *);
typedef void (*free_fn)(dns_ref);
typedef int (*fd_fn)(dns_ref);
int main(void){
    alarm(25);setvbuf(stdout,NULL,_IONBF,0);
    /* A real SDK socket reference ensures its provider is linked. The
       absent network manager must remain a visible runtime limitation. */
    errno=0;
    int socket_fd=socket(AF_UNIX,SOCK_STREAM,0);
    int socket_error=errno;
    printf("BONJOUR_SDK_SOCKET_BASELINE fd=%d errno=%d\n",socket_fd,socket_error);
    if(socket_fd>=0)close(socket_fd);
    void *h=dlopen("/proc/boot/libdns_sd.so.1",RTLD_NOW|RTLD_LOCAL);
    if(!h){const char *e=dlerror();printf("BONJOUR_LOAD_FAIL %s\n",e?e:"unknown");return 2;}
    void *a=dlsym(h,"DNSServiceCreateConnection"),*b=dlsym(h,"DNSServiceRefDeallocate"),*c=dlsym(h,"DNSServiceRefSockFD");
    create_fn create=NULL;free_fn dispose=NULL;fd_fn getfd=NULL;
    if(!a||!b||!c||sizeof a!=sizeof create||sizeof b!=sizeof dispose||sizeof c!=sizeof getfd){puts("BONJOUR_REQUIRED_EXPORT_MISSING");return 3;}
    memcpy(&create,&a,sizeof a);memcpy(&dispose,&b,sizeof b);memcpy(&getfd,&c,sizeof c);
    dns_ref ref=NULL;
    int32_t rc=create(&ref);
    printf("BONJOUR_CREATE_CONNECTION_RESULT code=%ld has_ref=%d\n",(long)rc,ref!=NULL);
    if(rc==0){
        if(!ref)return 4;
        int fd=getfd(ref);printf("BONJOUR_DAEMON_SOCKET fd=%d\n",fd);
        dispose(ref);
        if(fd<0)return 5;
        puts("BONJOUR_DAEMON_CONNECTION_PASS");
    }else{
        if(ref){puts("BONJOUR_UNEXPECTED_FAILURE_HANDLE");return 6;}
        puts("BONJOUR_DAEMON_CONNECTION_BLOCKED");
    }
    if(dlclose(h)!=0)return 7;
    puts("BONJOUR_ACTUAL_CALL_COMPLETE_NO_SERVICE_REGISTRATION_NO_PHONE");
    alarm(0);return 0;
}
