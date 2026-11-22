#!/usr/bin/env bash
# QNX65 ARMv7 build; emits only REVIEW_ONLY results. Needs real luka-dev base-env toolchain.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
P="$ROOT/project"
OUT="$ROOT/qnx_build_result"
mkdir -p "$OUT/objects" "$OUT/logs"
LOG="$OUT/logs/native_build.log"
exec > >(tee "$LOG") 2>&1
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
RE=arm-unknown-nto-qnx6.5.0eabi-readelf
if ! command -v "$RE" >/dev/null; then RE=readelf; fi
printf 'Build target: QNX Neutrino 6.5 ARMv7, RESEARCH ONLY\n'
printf 'Toolchain: '; command -v "$CC"
"$CC" --version | head -1
printf 'SDK: QNX_HOST=%s QNX_TARGET=%s\n' "${QNX_HOST:-missing}" "${QNX_TARGET:-missing}"
test -e "${QNX_TARGET:-/nonexistent}/armle-v7/lib/libc.so.3" || { echo 'ERROR: QNX libc.so.3 missing'; exit 42; }
"$CC" -dM -E -x c /dev/null | grep -E '(__QNXNTO__|__QNX__|__NTO__)' | head -10
mapfile -t sources < <(find "$P/source/src" "$P/source/legacy_mu1005/airplay" "$P/source/legacy_mu1005/bluetooth" -maxdepth 1 -name '*.c' | sort)
[ "${#sources[@]}" = 40 ] || { echo "ERROR: expected 39 C files got ${#sources[@]}"; exit 44; }
flags=(-DAU57X_FAST_CONNECT_EXPERIMENT=1 -std=c99 -O2 -Wall -Wextra -Werror -fPIC -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -mno-unaligned-access
       -I"$P/source/src" -I"$P/source/legacy_mu1005/airplay" -I"$P/source/legacy_mu1005/bluetooth")
for s in "${sources[@]}"; do
   b=$(basename "$s" .c)
   printf 'COMPILE %s\n' "$b"
   "$CC" "${flags[@]}" -c "$s" -o "$OUT/objects/$b.o"
done
# Source-only original SD recovery controller, compiled separately.  Never linked
# into the CarPlay radio/receiver library or exposed as an install payload.
mkdir -p "$OUT/review_txn"
for s in "$ROOT/tools/review_txn/sha256.c" "$ROOT/tools/review_txn/txn.c"; do
    b=$(basename "$s" .c)
    printf 'COMPILE REVIEW_ONLY SD_TRANSACTION %s\n' "$b"
    "$CC" "${flags[@]}" -c "$s" -o "$OUT/review_txn/$b.o"
done
printf 'LINK QNX .so\n'
# Keep a direct NEEDED entry for QNX libc.so.3: the ordinary
# -shared link permits unresolved externals and drops libc on this SDK.
# --no-as-needed only applies to this one explicit QNX system library.
"$CC" -shared -Wl,-Bsymbolic-functions -Wl,--hash-style=sysv \
  -o "$OUT/libau57x_r24_native_qnx_REVIEW_ONLY.so" \
  "$OUT"/objects/*.o "$OUT/review_txn/sha256.o" -Wl,--no-as-needed -lc -lsocket -Wl,--as-needed
$RE -d "$OUT/libau57x_r24_native_qnx_REVIEW_ONLY.so" | tee "$OUT/logs/dynamic_deps.txt"
"$RE" -h -A -d "$OUT/libau57x_r24_native_qnx_REVIEW_ONLY.so" > "$OUT/logs/readelf.txt"
if ! "$RE" -d "$OUT/libau57x_r24_native_qnx_REVIEW_ONLY.so" | grep -q "Shared library: \[libsocket.so.3\]"; then
  echo "ERROR: native socket library dependency missing"; exit 45
fi
file "$OUT/libau57x_r24_native_qnx_REVIEW_ONLY.so" | tee "$OUT/logs/file.txt"
# R92 real QNX LD_PRELOAD entry library; intentionally does not call OEM SDK.
# Stock RF/SDP callback dispatch still requires a verified native owner.
printf 'LINK R92 QNX native LD_PRELOAD shim\n'
"$CC" -shared -Wl,--hash-style=sysv \
 -o "$OUT/libau57x_bt_preload_qnx_REVIEW_ONLY.so" \
 "$OUT/objects/bt_preload_probe.o" "$OUT/review_txn/sha256.o" \
 -Wl,--no-as-needed -lc -Wl,--as-needed
"$RE" -h -d "$OUT/libau57x_bt_preload_qnx_REVIEW_ONLY.so" \
 > "$OUT/logs/r92_preload_readelf.txt"
"$RE" -d "$OUT/libau57x_bt_preload_qnx_REVIEW_ONLY.so" | grep -q 'Shared library: \[libc.so.3\]' || { echo 'R92 preload missing QNX libc'; exit 47; }
file "$OUT/libau57x_bt_preload_qnx_REVIEW_ONLY.so" | tee "$OUT/logs/r92_preload_file.txt"

echo 'LINKED: QNX native file generated. NO_GO for installation; not runtime validated.'
