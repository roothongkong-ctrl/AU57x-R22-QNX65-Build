#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
TMP="$(mktemp -d)";trap 'rm -rf "$TMP"' EXIT
cc -std=c99 -O2 -Wall -Wextra -Werror -DAU57X_R98_HOST_TEST=1 \
   -Iproject/source/legacy_mu1005/bluetooth \
   project/source/legacy_mu1005/bluetooth/bt_iap2_owner_handoff.c \
   project/source/legacy_mu1005/bluetooth/rf_arg_experiment.c \
   tests/test_r98_iap2_owner_handoff.c -o "$TMP/owner_fixture"
"$TMP/owner_fixture"
