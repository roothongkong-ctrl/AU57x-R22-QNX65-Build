#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
c=project/source/src
a=project/source/legacy_mu1005/airplay
b=project/source/legacy_mu1005/bluetooth
p=integration/mu1005_si_native/ap_live
out=$(mktemp -d);trap 'rm -rf "$out"' EXIT
cc -std=gnu99 -O1 -g -Wall -Wextra -Werror -Wno-unused-function -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie -DMU1005_QNX_FIRST_LINK_FIXTURE -DMU_QNX_LIVE_AP_FALLBACK -DMU_AP_REFRESH_TEST -rdynamic -I"$c" -I"$a" -I"$b" tests/test_cloud_pipeline.c "$p"/{ap_live,ap_refresh}.c "$c"/{cp_cloud_pipeline,cp_wcp_autoconnect,cp_wcp_handoff,cp_radio_policy,cp_session_handoff,cp_step04_session,cp_step04_link,cp_step04_iap2_auth,cp_step04_stock_mfi,cp_oem_boundary,cp_wireless,cp_event,cp_csm}.c "$a"/{mu1005_r34_ap_server,native_exports,cprx_service,semantic}.c "$b/mu1005_rf_ingress.c" -ldl -o "$out/live-ap"
"$out/live-ap"
