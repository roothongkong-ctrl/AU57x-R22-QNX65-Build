#include "cp_wcp_autoconnect.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct fixture {int owner,paired,wired,linked,sdp,open_rc,cancel_rc;int opens,cancels;uint32_t g;};
static int own(void *p,uint32_t g){struct fixture *s=p;return s->owner&&s->g==g;}
static int paired(void *p,uint32_t g){struct fixture*s=p;return s->paired&&s->g==g;}
static int wired(void *p){return ((struct fixture*)p)->wired;}
static int linked(void *p,uint32_t g){struct fixture*s=p;return s->linked&&s->g==g;}
static int sdp(void *p,uint32_t g){struct fixture*s=p;return s->g==g?s->sdp:-1;}
static int op(void *p,uint32_t g){struct fixture*s=p;assert(s->g==g);s->opens++;return s->open_rc;}
static int cancel(void *p,uint32_t g){struct fixture*s=p;assert(s->g==g);s->cancels++;return s->cancel_rc;}
static void setup(struct cp_wcp_autoconnect *s,struct fixture *f){
 struct cp_wcp_bt_ops o={f,own,paired,wired,linked,sdp,op,cancel};
 memset(f,0,sizeof *f);f->g=7;f->owner=f->paired=f->wired=f->sdp=f->open_rc=f->cancel_rc=1;
 assert(cp_wcp_bt_init(s,&o)==CP_WCP_OK);
}
int main(void){
 struct cp_wcp_autoconnect s;struct fixture f;
 setup(&s,&f);
 assert(cp_wcp_bt_acl_up(&s,7,100)==CP_WCP_OK);
 assert(cp_wcp_bt_tick(&s,7,4099)==CP_WCP_WAIT);
 assert(f.opens==0);
 assert(cp_wcp_bt_tick(&s,6,4100)==CP_WCP_STALE);
 assert(cp_wcp_bt_tick(&s,7,4100)==CP_WCP_WAIT);
 assert(f.opens==1&&s.state==CP_WCP_OPENING);
 f.linked=1;
 assert(cp_wcp_bt_tick(&s,7,4101)==CP_WCP_LINK_READY);
 assert(s.state==CP_WCP_READY&&s.attempts==1);
 assert(cp_wcp_bt_abort(&s,7,1)==CP_WCP_OK);
 assert(s.state==CP_WCP_BLOCKED); /* fresh ACL event needed */
 assert(cp_wcp_bt_acl_up(&s,7,4102)==CP_WCP_DENIED);
 setup(&s,&f);
 assert(cp_wcp_bt_acl_up(&s,7,0)==CP_WCP_OK);
 assert(cp_wcp_bt_tick(&s,7,4000)==CP_WCP_WAIT);
 f.cancel_rc=0;
 assert(cp_wcp_bt_tick(&s,7,16000)==CP_WCP_WAIT);
 assert(s.state==CP_WCP_RESTORE_PENDING);
 assert(cp_wcp_bt_acl_up(&s,8,16001)==CP_WCP_DENIED);
 f.cancel_rc=1;
 assert(cp_wcp_bt_restore_tick(&s,7)==CP_WCP_OK);
 assert(s.state==CP_WCP_BLOCKED);
 setup(&s,&f);
 f.sdp=0;
 assert(cp_wcp_bt_acl_up(&s,7,0)==CP_WCP_OK);
 assert(cp_wcp_bt_tick(&s,7,4000)==CP_WCP_WAIT&&f.opens==0);
 assert(s.deadline_ms==34000);
 f.sdp=1;
 assert(cp_wcp_bt_tick(&s,7,34000)==CP_WCP_WAIT&&f.opens==1);
 setup(&s,&f);
 assert(cp_wcp_bt_acl_up(&s,7,0)==CP_WCP_OK);
 assert(cp_wcp_bt_tick(&s,7,4000)==CP_WCP_WAIT);
 assert(cp_wcp_bt_tick(&s,7,16000)==CP_WCP_WAIT);
 assert(cp_wcp_bt_tick(&s,7,21000)==CP_WCP_WAIT);
 assert(cp_wcp_bt_tick(&s,7,33000)==CP_WCP_WAIT);
 assert(cp_wcp_bt_tick(&s,7,38000)==CP_WCP_WAIT);
 assert(cp_wcp_bt_tick(&s,7,50000)==CP_WCP_DENIED);
 assert(f.opens==3&&f.cancels==3);
 setup(&s,&f);
 assert(cp_wcp_bt_acl_up(&s,7,0)==CP_WCP_OK);
 f.wired=0;
 assert(cp_wcp_bt_tick(&s,7,1000)==CP_WCP_DENIED);
 assert(s.state==CP_WCP_BLOCKED&&f.opens==0);
 setup(&s,&f);
 assert(cp_wcp_bt_acl_up(&s,7,UINT64_MAX-2000)==CP_WCP_CLOCK);
 assert(s.state==CP_WCP_IDLE);
 setup(&s,&f);
 assert(cp_wcp_bt_acl_up(&s,7,100)==CP_WCP_OK);
 assert(cp_wcp_bt_tick(&s,7,99)==CP_WCP_CLOCK);
 assert(cp_wcp_bt_tick(&s,7,4100)==CP_WCP_WAIT);
 assert(cp_wcp_bt_abort(&s,7,0)==CP_WCP_OK);
 assert(s.state==CP_WCP_IDLE);
 setup(&s,&f);f.paired=0;
 assert(cp_wcp_bt_acl_up(&s,7,0)==CP_WCP_DENIED);
 assert(f.opens==0);
 printf("R87 WCP car-initiated original iAP2 state machine: PASS\n");
 return 0;
}
