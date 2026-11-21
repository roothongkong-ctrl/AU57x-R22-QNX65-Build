#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/cloud12_result"
(
 cd "$ROOT/tools/review_txn"
 CC=gcc bash run_host.sh
) 2>&1 | tee "$ROOT/cloud12_result/sd_txn_67cases.log"
