#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${CC:-/opt/qnx650/host/linux/x86/usr/bin/arm-unknown-nto-qnx6.5.0eabi-gcc}"
RE="${RE:-/opt/qnx650/host/linux/x86/usr/bin/arm-unknown-nto-qnx6.5.0eabi-readelf}"
OUT="$ROOT/qnx_build_result/x43_native"
mkdir -p "$OUT"
INC="-Isrc/bluetooth -Iproject/source/legacy_mu1005/bluetooth -Ix3/src"
CFLAGS="-std=c11 -O2 -Wall -Wextra -Werror -fPIC -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp $INC"
SRC=(
  src/bluetooth/native_binding.c
  src/bluetooth/bt_route_gate.c
  x3/src/mu1005_owner_gate.c
  project/source/legacy_mu1005/bluetooth/native_event.c
  project/source/legacy_mu1005/bluetooth/inbound_r5.c
  project/source/legacy_mu1005/bluetooth/inbound.c
  project/source/legacy_mu1005/bluetooth/mu1005_rf_ingress.c
  project/source/legacy_mu1005/bluetooth/mu1005_ipc_stream.c
  project/source/legacy_mu1005/bluetooth/mu1005_socket_pump.c
  project/source/legacy_mu1005/bluetooth/rf_arg_experiment.c
  project/source/legacy_mu1005/bluetooth/bt_iap2_owner_handoff.c
)
OBJS=()
i=0
for src in "${SRC[@]}"; do
  obj="$OUT/$(basename "$src" .c)_$i.o"
  "$CC" $CFLAGS -c "$src" -o "$obj"
  OBJS+=("$obj")
  i=$((i+1))
done
"$CC" -shared -Wl,--hash-style=sysv -Wl,--no-undefined -o "$OUT/libau57x_x43_native_REVIEW_ONLY.so" "${OBJS[@]}" -lc
"$RE" -h -d "$OUT/libau57x_x43_native_REVIEW_ONLY.so" > "$OUT/readelf.txt"
grep -q "Shared library: \[libc.so.3\]" "$OUT/readelf.txt"
python3 project/tools/qnx_elf_gate.py --candidate "$OUT/libau57x_x43_native_REVIEW_ONLY.so" --output "$OUT/elf_gate.json"
echo "X4.3 full native_binding QNX ARM32 compile/link PASS — NO OEM owner provider, NOT installable"
