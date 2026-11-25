#include "bt_notify_bridge.h"
#include <assert.h>
#include <stdio.h>
typedef struct {MuNotifyGate g;int owner,enabled,original,after,reenter,disable,trace;} F;
static int owner(void*p){return ((F*)p)->owner;}
static int enabled(void*p){return ((F*)p)->enabled;}
static void orig(void*p,unsigned char v){
 F*f=p;assert(v==1);f->original++;f->trace=f->trace*10+1;
 if(f->reenter){f->reenter=0;mu_notify_call(&f->g,orig,f,1);}
 if(f->disable)f->enabled=0;
}
static void after(void*p){F*f=p;f->after++;f->trace=f->trace*10+2;assert(f->g.inside==1);}
int main(void){
 F f={0};f.g=(MuNotifyGate){&f,owner,enabled,after,0};f.owner=f.enabled=1;
 mu_notify_call(&f.g,orig,&f,1);assert(f.original==1&&f.after==1&&f.trace==12&&!f.g.inside);
 f.trace=0;f.reenter=1;mu_notify_call(&f.g,orig,&f,1);assert(f.original==3&&f.after==2&&f.trace==112&&!f.g.inside);
 f.trace=0;f.owner=0;mu_notify_call(&f.g,orig,&f,1);assert(f.trace==1&&f.after==2);
 f.owner=1;f.trace=0;f.disable=1;mu_notify_call(&f.g,orig,&f,1);assert(f.trace==1&&f.after==2);
 assert(mu_bt_notify_bind(NULL)==-1&&mu_bt_notify_unbind()==-1);
 puts("PASS OEM notify first, outermost owner dispatch only, revoked readiness preserves original (host fixture)");
}
