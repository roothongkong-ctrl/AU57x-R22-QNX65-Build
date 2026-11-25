#define _POSIX_C_SOURCE 200809L
#include "bt_socket_worker.h"
#include <assert.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
static int take(MuBtSocketWorker*w){
 int fd=-1;struct timespec pause={0,10000000L};
 for(int i=0;i<300;i++){int rc=mu_bt_worker_poll(w,&fd);assert(rc>=0);if(rc==1)return fd;(void)nanosleep(&pause,NULL);}
 return -1;
}
int main(void){
 struct sockaddr_in a={0};socklen_t n=sizeof a;int l=socket(AF_INET,SOCK_STREAM,0);
 assert(l>=0);a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(!bind(l,(struct sockaddr*)&a,sizeof a)&&!listen(l,4)&&!getsockname(l,(struct sockaddr*)&a,&n));
 MuBtSocketWorker w={0};assert(mu_bt_worker_start(&w,ntohs(a.sin_port))==0);
 for(int i=0;i<2;i++){
  int fd=take(&w);assert(fd>=0);int peer=accept(l,NULL,NULL);assert(peer>=0);
  assert(fcntl(fd,F_GETFL,0)&O_NONBLOCK);assert(fcntl(fd,F_GETFD)&FD_CLOEXEC);
  assert(send(peer,"x",1,0)==1);char x;struct timespec p={0,1000000};
  int got=0;for(int j=0;j<1000&&!got;j++){if(recv(fd,&x,1,0)==1)got=1;else nanosleep(&p,NULL);}
  assert(got&&x=='x');close(fd);close(peer);
 }
 assert(mu_bt_worker_stop(&w)==0&&!w.running);close(l);
 puts("PASS worker real TCP preparation, nonblocking owner mailbox, two independent sockets");
}
