#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="${TMPDIR:-/tmp}/au57x-x43-native"
mkdir -p "$TMP"
INC="-I$ROOT/src/bluetooth -I$ROOT/project/source/legacy_mu1005/bluetooth -I$ROOT/x3/src"
cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 -DAU57X_R98_HOST_TEST=1 $INC \
  "$ROOT/x3/src/mu1005_owner_gate.c" \
  "$ROOT/src/bluetooth/bt_route_gate.c" \
  "$ROOT/project/source/legacy_mu1005/bluetooth/rf_arg_experiment.c" \
  "$ROOT/project/source/legacy_mu1005/bluetooth/bt_iap2_owner_handoff.c" \
  "$ROOT/tests/test_r98_iap2_owner_handoff.c" -o "$TMP/test_r98_gate"
"$TMP/test_r98_gate"
cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 $INC -c \
  "$ROOT/src/bluetooth/native_binding.c" -o "$TMP/native_binding_host_stub.o"
echo "NATIVE_BINDING_HOST_STUB_COMPILE_PASS (QNX ARM32 branch NOT compiled)"
bash "$ROOT/ci/host_mu1005_rf_scn_extended.sh"
