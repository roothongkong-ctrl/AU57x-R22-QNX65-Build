/* GPL-3.0-only. Host fixture, not a real BlueSDK or OEM completion. */
#include "byte_contract.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fixture {struct cp_bt_bridge b;struct cp_step2_transport t;int accepted,mode;};
static int verify(void *x,uint32_t g){struct fixture *f=x;return g==f->b.generation&&f->b.connected;}
static int submit(void *x,uint32_t g,const uint8_t *p,size_t n){
    struct fixture *f=x;assert(g==f->b.generation&&n==3&&memcmp(p,"abc",3)==0);
    f->accepted++;return 0;
}
static int finish(void *x,struct cp_step2_transport *t,uint32_t g,size_t n){
    struct fixture *f=x;assert(n==3&&f->b.tx_pending);
    if(f->mode==1)return 1; /* dishonest/early success: still pending */
    if(f->mode==2)return 0; /* timeout */
    assert(cp_step2_transport_tx_done(t,g,1)==CP_BT_OK);
    return 1;
}
static void setup(struct fixture *f){
    struct cp_step2_owner o;
    memset(f,0,sizeof(*f));cp_bt_bridge_init(&f->b);cp_step2_transport_init(&f->t);
    assert(cp_bt_bridge_event(&f->b,CP_BT_REQUEST,0,0)==CP_BT_OK);
    assert(cp_bt_request_decided(&f->b,1)==CP_BT_OK);
    assert(cp_bt_bridge_event(&f->b,CP_BT_CONNECTED,0,0)==CP_BT_OK);
    o.opaque=f;o.verify=verify;o.submit_async=submit;
    assert(cp_step2_transport_bind(&f->t,&f->b,&o,f->b.generation)==CP_BT_OK);
}
int main(void){
    struct fixture f;uint8_t out[5];const uint8_t *p=(const uint8_t *)"abc";
    setup(&f);assert(route3_send(&f.t,p,3,0,&f)==-1&&f.accepted==0);
    assert(route3_send(&f.t,p,3,finish,&f)==3&&f.accepted==1&&!f.b.tx_pending);
    assert(route3_recv_now(&f.t,out,sizeof(out))==0);
    assert(cp_step2_transport_rx(&f.t,f.b.generation,p,3)==CP_BT_OK);
    assert(route3_recv_now(&f.t,out,2)==2&&memcmp(out,"ab",2)==0);
    assert(route3_recv_now(&f.t,out,5)==1&&out[0]=='c');
    assert(route3_recv_now(&f.t,out,5)==0);
    f.mode=1;assert(route3_send(&f.t,p,3,finish,&f)==-1&&f.b.closed);
    assert(route3_send(&f.t,p,3,finish,&f)==-1&&f.accepted==2);
    assert(route3_recv_now(&f.t,out,5)==-1);
    setup(&f);f.mode=2;assert(route3_send(&f.t,p,3,finish,&f)==-1&&f.b.closed);
    puts("ROUTE3_BYTE_COUNT_COMPLETION_FIXTURE_PASS_NOT_OEM");return 0;
}
