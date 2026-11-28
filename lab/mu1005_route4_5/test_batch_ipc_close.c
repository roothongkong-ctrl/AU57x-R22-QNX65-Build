#include "cp_step2_ipc_link.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static int peer(void*p,const uint8_t*m,uint32_t g){(void)p;return m[0]==1&&g==7;}
static int emit(void*p,const uint8_t*d,size_t n){(void)p;(void)d;(void)n;return 0;}
int main(int argc,char**argv){
 assert(argc==2);int mode=atoi(argv[1]);assert(mode>=0&&mode<=2);
 struct cp_step2_ipc_link l;struct cp_step2_ipc_owner o={0,peer,peer,emit};
 uint8_t open[]={1,0,6,0,7,0,0,0,1,2,3,4,5,6};
 uint8_t close_more[]={3,0,0,0,7,0,0,0,2};
 uint8_t data[]={2,0,1,0,7,0,0,0,0x55};
 uint8_t ack[]={4,0,0,0,7,0,0,0};
 cp_step2_ipc_init(&l,&o);
 assert(cp_step2_ipc_feed(&l,open,sizeof open)==CP_BT_OK);
 int r;
 if(mode==0)r=cp_step2_ipc_feed(&l,close_more,sizeof close_more);
 else if(mode==1){l.rx_frames=ULONG_MAX;r=cp_step2_ipc_feed(&l,data,sizeof data);}
 else {l.tx_confirmed=ULONG_MAX;assert(cp_step2_ipc_write(&l,data+8,1)==CP_BT_OK);r=cp_step2_ipc_feed(&l,ack,sizeof ack);}
 assert(r==CP_BT_PROTOCOL_ERROR&&l.poisoned&&l.finished&&!l.connected&&!l.clean_disconnect);
 printf("IPC_TERMINAL_BOUNDARY_%d_PASS_FIXTURE_ONLY\n",mode);
}
