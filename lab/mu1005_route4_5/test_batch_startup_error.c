#include "bt_startup_watch.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct {int event,acquire,enters;} F;
static int take(void*p){F*f=p;return f->event;}
static int enter(void*p){F*f=p;f->enters++;return f->acquire;}
static void leave(void*p){(void)p;assert(0);}
static int read_live(void*p,uint32_t a,void*b,size_t n){(void)p;(void)a;(void)b;(void)n;assert(0);return 0;}
static void ready(void*p,uint32_t a){(void)p;(void)a;assert(0);}
int main(int argc,char**argv){
 assert(argc==2);int mode=atoi(argv[1]);assert(mode>=0&&mode<=2);
 MuBtStartupWatch w={2};F f={mode==0?-1:mode==1?2:1,-1,0};
 int r=mu_bt_startup_watch_tick_source(&w,take,&f,3,enter,leave,read_live,ready,&f);
 assert(r==-1&&!w.remaining&&f.enters==(mode==2));
 printf("STARTUP_PROVIDER_ERROR_%d_PASS_FIXTURE_ONLY\n",mode);
}
