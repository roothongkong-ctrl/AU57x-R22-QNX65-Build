#include "../project/source/legacy_mu1005/bluetooth/bt_route_gate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Au100Evidence ready(void){
    Au100Evidence e = {0};
    e.stock_image_pinned=e.original_config_backed_up=1;
    e.watchdog_and_recovery_installed=e.wired_carplay_untouched=1;
    e.factory_iap2_config_enabled=e.factory_endpoint_live=e.factory_rfcomm_bound=1;
    e.user_authorized_graft=e.exact_btstack_process_proven=1;
    e.stock_blue_sdk_thread_proven=e.rf_register_profile_proven=1;
    e.native_callback_owner_proven=e.factory_iap2_quiesced=1;
    e.original_rf_rollback_barrier_proven=1;
    return e;
}
#define CHECK(x) do {assert((x));count++;}while(0)
int main(void){
    int count=0;
    Au100Evidence e=ready();
    CHECK(au100_route_select(NULL)==AU100_NO_GO);
    CHECK(au100_route_select(&e)==AU100_FACTORY_IAP2_ROUTE);
    e.factory_rfcomm_bound=0;
    CHECK(au100_route_select(&e)==AU100_GRAFT_RFCOMM_ROUTE);
    e.user_authorized_graft=0;
    CHECK(au100_route_select(&e)==AU100_NO_GO);
    e=ready();e.factory_endpoint_live=0;e.stock_blue_sdk_thread_proven=0;
    CHECK(au100_route_select(&e)==AU100_NO_GO);
    e=ready();e.factory_endpoint_live=0;e.rf_register_profile_proven=0;
    CHECK(au100_route_select(&e)==AU100_NO_GO);
    e=ready();e.factory_endpoint_live=0;e.native_callback_owner_proven=0;
    CHECK(au100_route_select(&e)==AU100_NO_GO);
    e=ready();e.factory_endpoint_live=0;e.factory_iap2_quiesced=0;
    CHECK(au100_route_select(&e)==AU100_NO_GO);
    e=ready();e.factory_endpoint_live=0;e.original_rf_rollback_barrier_proven=0;
    CHECK(au100_route_select(&e)==AU100_NO_GO);
    e=ready();e.factory_endpoint_live=0;e.original_config_backed_up=0;
    CHECK(au100_route_select(&e)==AU100_NO_GO);
    e=ready();e.factory_endpoint_live=0;e.wired_carplay_untouched=0;
    CHECK(au100_route_select(&e)==AU100_NO_GO);
    e=ready();e.factory_endpoint_live=0;e.watchdog_and_recovery_installed=0;
    CHECK(au100_route_select(&e)==AU100_NO_GO);
    e=ready();e.factory_endpoint_live=0;e.exact_btstack_process_proven=0;
    CHECK(au100_route_select(&e)==AU100_NO_GO);
    e=ready();e.factory_rfcomm_bound=0;
    CHECK(au100_route_select(&e)==AU100_GRAFT_RFCOMM_ROUTE);
    e=ready();e.factory_rfcomm_bound=0;e.native_callback_owner_proven=2;
    CHECK(au100_route_select(&e)==AU100_NO_GO);
    CHECK(strcmp(au100_route_name(AU100_GRAFT_RFCOMM_ROUTE),"GRAFT_RUNTIME_ELIGIBLE")==0);
    CHECK(strcmp(au100_route_name(AU100_NO_GO),"NO_GO")==0);
    printf("R100_ROUTE_GATE_%d_STRICT_ASSERTIONS_PASS_NOT_A_CAR_TEST\n",count);
    return 0;
}
