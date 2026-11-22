#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void *thread_main(void *arg){return arg;}
int main(void){pthread_t t;void *ret=NULL;char token='T';
 if(pthread_create(&t,NULL,thread_main,&token)!=0)return 14;
 if(pthread_join(t,&ret)!=0||ret!=&token)return 15;
 if(pthread_create(&t,NULL,thread_main,&token)!=0)return 16;
 if(pthread_join(t,&ret)!=0||ret!=&token)return 17;
 puts("fake-btstack-thread-forwarding-ok");return 0;
}
