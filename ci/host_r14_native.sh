#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/project/source/legacy_mu1005/bluetooth"
OUT="$ROOT/cloud12_result"
mkdir -p "$OUT"
for name in test_withdraw test_native_event test_ir5; do
  gcc -std=c99 -O1 -g -Wall -Wextra -Werror -D_DEFAULT_SOURCE \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$SRC" "$ROOT/tests/r14_${name}.c" \
    "$SRC/inbound.c" "$SRC/inbound_r5.c" "$SRC/native_event.c" \
    "$SRC/native_binding.c" "$SRC/mu1005_rf_ingress.c" \
    "$SRC/mu1005_ipc_stream.c" "$SRC/mu1005_socket_pump.c" \
    -o "$OUT/r45_${name}"
  "$OUT/r45_${name}" | tee "$OUT/r45_${name}.log"
done

# Exercise the native registration credit path separately from old zero-credit fixture.
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -D_DEFAULT_SOURCE -DINITIAL_RX=4 -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie -I"$SRC" "$ROOT/tests/r14_test_ir5.c" "$SRC/inbound.c" "$SRC/inbound_r5.c" "$SRC/native_event.c" "$SRC/native_binding.c" "$SRC/mu1005_rf_ingress.c" "$SRC/mu1005_ipc_stream.c" "$SRC/mu1005_socket_pump.c" -o "$OUT/r45_test_ir5_native_credits"
"$OUT/r45_test_ir5_native_credits" | tee "$OUT/r45_native_credits.log"

# Native incoming/open identity continuity, even when MAC and authorization match.
"$OUT/r45_test_native_event" association-switch | tee "$OUT/r45_association_switch.log"
"$OUT/r45_test_native_event" revoked-peer | tee "$OUT/r45_revoked_peer.log"
