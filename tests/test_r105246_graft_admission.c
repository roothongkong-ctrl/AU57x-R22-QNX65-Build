#include "bt_iap2_owner_handoff.h"
#include <assert.h>
#include <stdio.h>
static int yes(void*p){(void)p;return 1;}
static int no(void*p){(void)p;return 0;}
static int rd(void*p,uint32_t a,void*b,size_t n){(void)p;(void)a;(void)b;(void)n;return 0;}
int main(void){
 Au100Evidence e={0};
 e.stock_image_pinned=e.original_config_backed_up=1;
 e.watchdog_and_recovery_installed=e.wired_carplay_untouched=1;
 e.user_authorized_graft=e.exact_btstack_process_proven=1;
 e.stock_blue_sdk_thread_proven=e.rf_register_profile_proven=1;
 e.native_callback_owner_proven=e.factory_iap2_quiesced=1;
 e.original_rf_rollback_barrier_proven=1;
 Au98OwnerProof p={0};
 p.original_image_verified=yes;p.on_original_iap2_owner=yes;p.route_evidence=&e;
 MuBtOwner o={0};o.owner_ready=yes;o.transport_ready=yes;o.read_live=rd;
 /* No factory service exists; this is the reason graft is needed. */
 assert(au98_graft_admission(&p,&o)==1);
 p.original_iap2_service_verified=no;
 assert(au98_graft_admission(&p,&o)==1);
 e.native_callback_owner_proven=0;assert(!au98_graft_admission(&p,&o));e.native_callback_owner_proven=1;
 e.rf_register_profile_proven=0;assert(!au98_graft_admission(&p,&o));e.rf_register_profile_proven=1;
 e.factory_iap2_quiesced=0;assert(!au98_graft_admission(&p,&o));e.factory_iap2_quiesced=1;
 e.original_rf_rollback_barrier_proven=0;assert(!au98_graft_admission(&p,&o));e.original_rf_rollback_barrier_proven=1;
 p.on_original_iap2_owner=no;assert(!au98_graft_admission(&p,&o));p.on_original_iap2_owner=yes;
 o.owner_ready=no;assert(!au98_graft_admission(&p,&o));o.owner_ready=yes;
 o.transport_ready=no;assert(!au98_graft_admission(&p,&o));o.transport_ready=yes;
 e.factory_iap2_config_enabled=e.factory_endpoint_live=e.factory_rfcomm_bound=1;
 assert(!au98_graft_admission(&p,&o));
 puts("R105246_GRAFT_NO_PREEXISTING_FACTORY_SERVICE_REQUIRED_PASS_FIXTURE_ONLY");
 return 0;
}
