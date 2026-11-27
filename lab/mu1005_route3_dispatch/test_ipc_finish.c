/* SDK/owner fixture, exercising real existing IPC parser and byte adapter. */
#include "ipc_finish.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint64_t now;unsigned emitted,steps,mode;} Fix;
static int verify(void*p,const uint8_t*m,uint32_t g){(void)p;return g==7&&m[0]==1;}
static int emit(void*p,const uint8_t*b,size_t n){Fix*f=p;assert(n==11&&b[0]==2&&b[4]==7&&!memcmp(b+8,"abc",3));f->emitted++;return 0;}
static uint64_t clock_ms(void*p){return ((Fix*)p)->now;}
static int step(void*p,struct cp_step2_ipc_link*l,uint64_t deadline){
 Fix*f=p;uint8_t ack[8]={4,0,0,0,7,0,0,0};
 assert(l->bridge.tx_pending&&deadline==10);
 ++f->steps;++f->now;
 if(f->mode==1){f->now=deadline;return 1;}
 if(f->mode==2)ack[4]=8;
 if(f->mode==3){ack[0]=3;}
 if(f->mode==4){assert(cp_step2_ipc_feed(l,ack,4)==CP_BT_OK);return cp_step2_ipc_feed(l,ack+4,4)==CP_BT_OK?1:-1;}
 return cp_step2_ipc_feed(l,ack,sizeof ack)==CP_BT_OK?1:-1;
}
static void setup(Fix*f,struct cp_step2_ipc_link*l,Route3IpcFinish*x,unsigned mode){
 uint8_t open[14]={1,0,6,0,7,0,0,0,1,2,3,4,5,6};
 struct cp_step2_ipc_owner o={f,verify,verify,emit};
 memset(f,0,sizeof(*f));f->mode=mode;cp_step2_ipc_init(l,&o);
 assert(cp_step2_ipc_feed(l,open,sizeof open)==CP_BT_OK);
 *x=(Route3IpcFinish){l,f,step,clock_ms,10,8};
}
int main(void){
 Fix f;struct cp_step2_ipc_link l;Route3IpcFinish x;
 const uint8_t*p=(const uint8_t*)"abc";
 setup(&f,&l,&x,0);assert(route3_ipc_send(&x,p,3)==3&&l.tx_confirmed==1&&f.emitted==1);
 setup(&f,&l,&x,4);assert(route3_ipc_send(&x,p,3)==3&&l.tx_confirmed==1);
 for(unsigned i=1;i<=3;i++){setup(&f,&l,&x,i);assert(route3_ipc_send(&x,p,3)==-1&&l.poisoned&&f.emitted==1);assert(route3_ipc_send(&x,p,3)==-1&&f.emitted==1);}
 setup(&f,&l,&x,0);f.now=10;assert(route3_ipc_send(&x,p,3)==-1&&!f.emitted);
 puts("ROUTE3_EXISTING_IPC_ACK_TO_BYTE_COUNT_PASS_FIXTURE_ONLY");return 0;
}
