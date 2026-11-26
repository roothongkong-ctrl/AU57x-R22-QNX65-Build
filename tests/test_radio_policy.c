/* Host-only radio policy invariants. No real Wi-Fi driver or live evidence. */
#include "cp_radio_policy.h"
#include <assert.h>
#include <stdio.h>
static void dual(void){
 struct cp_radio_policy r;enum cp_radio_band b=CP_RADIO_NONE;
 cp_radio_init(&r);
 assert(cp_radio_begin(&r,1,1,1,1,1)==CP_RADIO_OK);
 assert(cp_radio_select(&r,1,1,&b)==CP_RADIO_OK&&b==CP_RADIO_5);
 assert(cp_radio_connect_requested(&r,1,-1)==CP_RADIO_NO_OWNER);
 assert(cp_radio_connect_requested(&r,1,1)==CP_RADIO_OK);
 assert(cp_radio_association_result(&r,1,1,0,CP_RADIO_5,0)==CP_RADIO_PENDING);
 assert(cp_radio_select(&r,1,1,&b)==CP_RADIO_BAD_STATE);
 assert(cp_radio_association_result(&r,1,1,0,CP_RADIO_5,1)==CP_RADIO_PENDING);
 assert(cp_radio_select(&r,1,0,&b)==CP_RADIO_NO_OWNER);
 assert(cp_radio_select(&r,1,-1,&b)==CP_RADIO_NO_OWNER);
 assert(cp_radio_select(&r,1,2,&b)==CP_RADIO_NO_OWNER);
 assert(cp_radio_select(&r,1,1,&b)==CP_RADIO_OK&&b==CP_RADIO_24);
 assert(cp_radio_connect_requested(&r,1,1)==CP_RADIO_OK);
 assert(cp_radio_association_result(&r,1,1,1,CP_RADIO_5,0)==CP_RADIO_BAD_STATE);
 assert(cp_radio_association_result(&r,1,-1,1,CP_RADIO_24,0)==CP_RADIO_NO_OWNER);
 assert(cp_radio_association_result(&r,1,1,1,CP_RADIO_24,0)==CP_RADIO_OK);
 assert(r.phase==CP_RADIO_ASSOCIATED);
 assert(cp_radio_select(&r,1,1,&b)==CP_RADIO_BAD_STATE);
 assert(cp_radio_revoke(&r,1,1)==CP_RADIO_OK);
 assert(cp_radio_select(&r,1,1,&b)==CP_RADIO_BLOCKED);
 assert(cp_radio_new_epoch_ready(&r,1,0)==CP_RADIO_NO_OWNER);
 assert(cp_radio_new_epoch_ready(&r,1,1)==CP_RADIO_OK);
 assert(cp_radio_begin(&r,1,1,1,1,1)==CP_RADIO_STALE);
 assert(cp_radio_begin(&r,2,1,1,1,1)==CP_RADIO_OK);
 assert(cp_radio_select(&r,1,1,&b)==CP_RADIO_STALE);
}
static void only24(void){
 struct cp_radio_policy r;enum cp_radio_band b=CP_RADIO_NONE;
 cp_radio_init(&r);
 assert(cp_radio_begin(&r,10,1,0,1,1)==CP_RADIO_OK);
 assert(cp_radio_select(&r,10,1,&b)==CP_RADIO_OK&&b==CP_RADIO_24);
 assert(cp_radio_connect_requested(&r,10,1)==CP_RADIO_OK);
 assert(cp_radio_association_result(&r,10,1,0,CP_RADIO_24,1)==CP_RADIO_NO_FALLBACK);
 assert(cp_radio_new_epoch_ready(&r,10,1)==CP_RADIO_OK);
}
static void only5(void){
 struct cp_radio_policy r;enum cp_radio_band b=CP_RADIO_NONE;
 cp_radio_init(&r);
 assert(cp_radio_begin(&r,11,0,1,1,1)==CP_RADIO_OK);
 assert(cp_radio_select(&r,11,1,&b)==CP_RADIO_OK&&b==CP_RADIO_5);
 assert(cp_radio_connect_requested(&r,11,1)==CP_RADIO_OK);
 assert(cp_radio_association_result(&r,11,1,0,CP_RADIO_5,1)==CP_RADIO_NO_FALLBACK);
}
static void reject(void){
 struct cp_radio_policy r;enum cp_radio_band b=CP_RADIO_NONE;
 cp_radio_init(&r);
 assert(cp_radio_begin(&r,7,0,0,1,1)==CP_RADIO_BLOCKED);
 assert(cp_radio_begin(&r,7,1,1,0,1)==CP_RADIO_INVALID);
 assert(cp_radio_begin(&r,7,1,1,1,0)==CP_RADIO_INVALID);
 assert(cp_radio_begin(&r,7,-1,1,1,1)==CP_RADIO_INVALID);
 assert(cp_radio_begin(&r,7,2,1,1,1)==CP_RADIO_INVALID);
 assert(cp_radio_begin(&r,7,1,1,1,1)==CP_RADIO_OK);
 assert(cp_radio_connect_requested(&r,7,1)==CP_RADIO_BAD_STATE);
 assert(cp_radio_select(&r,7,1,&b)==CP_RADIO_OK&&b==CP_RADIO_5);
 assert(cp_radio_connect_requested(&r,7,1)==CP_RADIO_OK);
 assert(cp_radio_association_result(&r,7,1,1,CP_RADIO_24,0)==CP_RADIO_BAD_STATE);
 assert(cp_radio_association_result(&r,7,1,1,CP_RADIO_5,1)==CP_RADIO_BAD_STATE);
 assert(cp_radio_association_result(&r,7,1,1,CP_RADIO_5,0)==CP_RADIO_OK);
 assert(cp_radio_new_epoch_ready(&r,7,1)==CP_RADIO_BAD_STATE);
}
static void first24(void){
 struct cp_radio_policy r;enum cp_radio_band b=CP_RADIO_NONE;
 cp_radio_init(&r);
 assert(cp_radio_channel_band(6)==CP_RADIO_24);
 assert(cp_radio_channel_band(36)==CP_RADIO_5);
 assert(cp_radio_channel_band(0)==CP_RADIO_NONE);
 assert(cp_radio_begin_preferred(&r,20,1,1,1,1,CP_RADIO_24)==CP_RADIO_OK);
 assert(cp_radio_select(&r,20,1,&b)==CP_RADIO_OK&&b==CP_RADIO_24);
 assert(cp_radio_connect_requested(&r,20,1)==CP_RADIO_OK);
 assert(cp_radio_association_result(&r,20,1,0,CP_RADIO_24,0)==CP_RADIO_PENDING);
 assert(cp_radio_select(&r,20,1,&b)==CP_RADIO_BAD_STATE);
 assert(cp_radio_association_result(&r,20,1,0,CP_RADIO_24,1)==CP_RADIO_PENDING);
 assert(cp_radio_select(&r,20,1,&b)==CP_RADIO_OK&&b==CP_RADIO_5);
 puts("PASS first-connect 2.4 GHz preference, verified fallback to 5 GHz");
}
int main(void){dual();only24();only5();reject();first24();
 puts("AU57X_R36_DUAL_BAND_POLICY_MODEL_PASS");return 0;}
