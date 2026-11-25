/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "bt_socket_worker.h"
#include "bt_iap2_owner_handoff.h"
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static void* worker(void*arg){
 MuBtSocketWorker*w=arg;
 pthread_mutex_lock(&w->lock);
 while(!w->stopping){
  while(!w->stopping&&!w->requested)pthread_cond_wait(&w->changed,&w->lock);
  if(w->stopping)break;
  w->requested=0;w->busy=1;pthread_mutex_unlock(&w->lock);
  int fd=au98_prepare_daemon_socket(w->port);
  /* Bounded retry cadence, outside owner and mailbox lock. */
  struct timespec delay={0,200000000L};(void)nanosleep(&delay,NULL);
  pthread_mutex_lock(&w->lock);
  w->busy=0;
  if(fd>=0){
   if(w->stopping||w->fd>=0)(void)close(fd);
   else w->fd=fd;
  }
 }
 pthread_mutex_unlock(&w->lock);return NULL;
}
int mu_bt_worker_start(MuBtSocketWorker*w,unsigned port){
 if(!w||w->running||!port||port>65535)return -1;
#if !defined(AU57X_R98_HOST_TEST) || AU57X_R98_HOST_TEST!=1
 if(port!=57342u)return -1;
#endif
 memset(w,0,sizeof *w);w->fd=-1;w->port=port;
 if(pthread_mutex_init(&w->lock,NULL)!=0)return -1;
 if(pthread_cond_init(&w->changed,NULL)!=0){pthread_mutex_destroy(&w->lock);return -1;}
 if(pthread_create(&w->thread,NULL,worker,w)!=0){pthread_cond_destroy(&w->changed);pthread_mutex_destroy(&w->lock);return -1;}
 w->running=1;return 0;
}
int mu_bt_worker_poll(MuBtSocketWorker*w,int*fd){
 if(!w||!w->running||!fd||*fd>=0)return -1;
 int rc=pthread_mutex_trylock(&w->lock);if(rc==EBUSY)return 0;if(rc)return -1;
 int out=0;
 if(w->stopping)out=-1;
 else if(w->fd>=0){*fd=w->fd;w->fd=-1;out=1;}
 else if(!w->busy&&!w->requested){w->requested=1;pthread_cond_signal(&w->changed);}
 pthread_mutex_unlock(&w->lock);return out;
}
int mu_bt_worker_stop(MuBtSocketWorker*w){
 if(!w||!w->running)return -1;
 if(pthread_mutex_lock(&w->lock)!=0)return -1;
 w->stopping=1;pthread_cond_signal(&w->changed);pthread_mutex_unlock(&w->lock);
 if(pthread_join(w->thread,NULL)!=0)return -1;
 if(w->fd>=0)(void)close(w->fd);
 pthread_cond_destroy(&w->changed);pthread_mutex_destroy(&w->lock);
 memset(w,0,sizeof *w);w->fd=-1;return 0;
}
