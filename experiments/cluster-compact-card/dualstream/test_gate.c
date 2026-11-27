#include <assert.h>
#include <stdio.h>
#include "gate.c"
static uint32_t seed=0xA6C71214u;
static uint32_t r(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(void){
 Gate g;Proof p={1,1,1,1,1,1,1};unsigned i;
 gate_init(&g);
 assert(gate_begin(&g,7)==G_OK);
 assert(gate_setup(&g,110,7,10)==G_STOCK);
 assert(gate_setup(&g,111,7,1111)==G_OK);
 assert(gate_setup(&g,112,7,1122)==G_REJECT);
 for(i=0;i<7;++i){
  Proof q=p;
  switch(i){
   case 0:q.opted_in=0;break;case 1:q.ios_112_observed=0;break;
   case 2:q.isolated_aes=0;break;case 3:q.receiver112_ready=0;break;
   case 4:q.unique_uuids=0;break;case 5:q.teardown_proven=0;break;
   default:q.card_mask_proven=0;
  }
  assert(gate_proof(&g,&q)==G_REJECT);
  assert(gate_setup(&g,112,7,1122)==G_REJECT);
 }
 assert(gate_proof(&g,&p)==G_OK);
 assert(gate_setup(&g,112,7,1111)==G_DUP);
 assert(gate_setup(&g,112,7,1122)==G_OK);
 assert(gate_setup(&g,112,7,1123)==G_DUP);
 assert(gate_frame(&g,111,7,1111,1000000,1000010)==G_OK);
 assert(gate_frame(&g,112,7,1122,1020000,1020010)==G_OK);
 assert(gate_view(&g,1020020)==V_DUAL);
 assert(gate_end(&g,110,7,10)==G_STOCK);
 assert(gate_view(&g,1020020)==V_DUAL);
 assert(gate_end(&g,112,6,1122)==G_STALE);
 assert(gate_end(&g,112,7,1122)==G_OK);
 assert(gate_view(&g,1020020)==V_MAP);
 assert(gate_setup(&g,112,7,1123)==G_OK);
 assert(gate_frame(&g,112,7,1122,1030000,1030000)==G_STALE);
 assert(gate_frame(&g,112,7,1123,1030000,1030000)==G_OK);
 assert(gate_view(&g,1030000)==V_DUAL); /* map is 30ms older, still synchronized */
 assert(gate_frame(&g,112,7,1123,1210000,1210010)==G_OK);
 assert(gate_view(&g,1210010)==V_MAP); /* 210ms mismatch: never overlay stale map/card */
 assert(gate_frame(&g,111,7,1111,1210100,1210110)==G_OK);
 assert(gate_view(&g,1210200)==V_DUAL);
 assert(gate_end(&g,111,7,1111)==G_OK);
 assert(gate_view(&g,1210200)==V_BLANK);
 assert(gate_invariant(&g));
 assert(gate_full_end(&g,7)==G_OK);
 assert(gate_setup(&g,112,7,1122)==G_STALE);
 for(i=0;i<20000;++i){
  uint64_t s=100+(i/101),a=20000+(i/101)*3;
  if(i%101==0)assert(gate_begin(&g,s)==G_OK);
  switch(r()%11){
   case 0:case 1:(void)gate_setup(&g,111,s,a);break;
   case 2:case 3:(void)gate_setup(&g,112,s,a+1);break;
   case 4:(void)gate_end(&g,112,s,a+1);break;
   case 5:(void)gate_end(&g,111,s,a);break;
   case 6:(void)gate_frame(&g,111,s,a,500000+(uint64_t)i*30000,500010+(uint64_t)i*30000);break;
   case 7:(void)gate_frame(&g,112,s,a+1,500000+(uint64_t)i*30000,500010+(uint64_t)i*30000);break;
   case 8:assert(gate_setup(&g,110,s,777)==G_STOCK);break;
   case 9:(void)gate_end(&g,112,s-1,a+1);break;
   default:(void)gate_view(&g,600000+(uint64_t)i*30000);break;
  }
  assert(gate_invariant(&g));
 }
 puts("PASS 112 gated; 110 isolated; 111 alive; stale/duplicate; 20000 operations");
 return 0;
}
