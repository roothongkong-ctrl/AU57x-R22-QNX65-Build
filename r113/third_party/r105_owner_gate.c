#include "r105_owner_gate.h"
#include <string.h>
void r105_gate_init(R105Gate *g){ if(g)memset(g,0,sizeof *g); }
static int proven(const R105Evidence *e){
 return e && e->generation && e->scn>=R105_SCN_MIN && e->scn<=R105_SCN_MAX &&
  e->firmware_verified==1u && e->owner_thread_verified==1u &&
  e->iap2_service_verified==1u && e->paired_authorized==1u &&
  e->original_mfi_provider_verified==1u && e->wired_active==0u;
}
int r105_gate_arm(R105Gate *g,const R105Evidence *e){
 if(!g||g->state!=R105_EMPTY||!proven(e))return -1;
 g->generation=e->generation;g->scn=e->scn;g->state=R105_ARMED;
 return 0;
}
int r105_gate_allow(const R105Gate *g,const R105Evidence *e){
 return g&&g->state==R105_ARMED && proven(e)&&
   g->generation==e->generation&&g->scn==e->scn;
}
void r105_gate_revoke(R105Gate *g){
 if(!g)return;
 g->state=R105_REVOKED;g->generation=0;g->scn=0;
}
