/* Host fixture for exact witness revocation in actual native dispatch guard. */
#include "bt_iap2_owner_handoff.h"
#include <assert.h>
#include <stdio.h>
typedef struct {int safe,thread,fd,allowed,calls;} F;
static int safe(void*p){return ((F*)p)->safe;}
static int thread(void*p){return ((F*)p)->thread;}
static int verify(void*p,int fd){F*f=p;f->calls++;return f->allowed&&fd==f->fd;}
int main(void){
 F f={1,1,8,1,0};Au98Handoff s={.state=AU98_PUBLISHED,.fd=8};
 s.hooks.opaque=&f;s.hooks.on_original_iap2_owner=thread;
 Au98DispatchPolicy p={.opaque=&f,.safe_dispatch=safe};
 assert(au98_guard_published_daemon(&s,&p,verify,&f)==0&&f.calls==1);
 f.thread=0;f.allowed=0;
 assert(au98_guard_published_daemon(&s,&p,verify,&f)==-1&&s.state==AU98_PUBLISHED&&f.calls==1);
 f.thread=1;f.safe=0;
 assert(au98_guard_published_daemon(&s,&p,verify,&f)==-1&&s.state==AU98_PUBLISHED&&f.calls==1);
 f.safe=1;
 assert(au98_guard_published_daemon(&s,&p,verify,&f)==-1&&s.state==AU98_DRAINING&&s.fd==8&&f.calls==2);
 f.allowed=1;assert(au98_guard_published_daemon(&s,&p,verify,&f)==0&&s.state==AU98_DRAINING&&f.calls==2);
 puts("PUBLISHED_EXACT_DAEMON_WITNESS_REVOKE_PASS_FIXTURE_ONLY");
}
