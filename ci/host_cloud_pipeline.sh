#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -D_DEFAULT_SOURCE -DMU_AP_REFRESH_TEST -rdynamic \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$ROOT/project/source/src" -I"$ROOT/project/source/legacy_mu1005/bluetooth" \
 -I"$ROOT/project/source/legacy_mu1005/airplay" \
 -I"$ROOT/integration/mu1005_si_native/ap_live" \
 "$ROOT/integration/mu1005_si_native/ap_live/ap_live.c" \
 "$ROOT/integration/mu1005_si_native/ap_live/ap_refresh.c" \
 "$ROOT/tests/test_cloud_pipeline.c" \
 "$ROOT/project/source/src/cp_cloud_pipeline.c" \
 "$ROOT/project/source/src/cp_wcp_autoconnect.c" \
 "$ROOT/project/source/src/cp_wcp_handoff.c" \
 "$ROOT/project/source/legacy_mu1005/airplay/mu1005_r34_ap_server.c" \
 "$ROOT/project/source/legacy_mu1005/airplay/native_exports.c" \
 "$ROOT/project/source/legacy_mu1005/airplay/cprx_service.c" \
 "$ROOT/project/source/legacy_mu1005/airplay/semantic.c" \
 "$ROOT/project/source/src/cp_radio_policy.c" \
 "$ROOT/project/source/src/cp_session_handoff.c" \
 "$ROOT/project/source/src/cp_step04_session.c" \
 "$ROOT/project/source/src/cp_step04_link.c" \
 "$ROOT/project/source/src/cp_step04_iap2_auth.c" \
 "$ROOT/project/source/src/cp_step04_stock_mfi.c" \
 "$ROOT/project/source/src/cp_oem_boundary.c" \
 "$ROOT/project/source/src/cp_wireless.c" \
 "$ROOT/project/source/src/cp_event.c" \
 "$ROOT/project/source/src/cp_csm.c" \
 "$ROOT/project/source/legacy_mu1005/bluetooth/mu1005_rf_ingress.c" \
 -ldl -o "$ROOT/cloud12_result/cloud_pipeline_host"
"$ROOT/cloud12_result/cloud_pipeline_host" | tee "$ROOT/cloud12_result/cloud_pipeline.log"
