#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
 -fno-omit-frame-pointer \
 -I"$ROOT/project/source/src" -I"$ROOT/project/source/legacy_mu1005/bluetooth" \
 -I"$ROOT/project/source/legacy_mu1005/airplay" \
 "$ROOT/project/source/src/cp_cloud_media.c" \
 "$ROOT/project/source/legacy_mu1005/airplay/mu1005_r34_ap_server.c" \
 "$ROOT/tests/test_cloud_media.c" -o "$ROOT/cloud12_result/cloud_media_host"
"$ROOT/cloud12_result/cloud_media_host" | tee "$ROOT/cloud12_result/cloud_media.log"
