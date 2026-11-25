#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
QNX_BIN="${QNX_HOST:-/opt/qnx650/host/linux/x86}/usr/bin"
PATH="$QNX_BIN:$PATH"; export PATH
CC=${QNX_CC:-$QNX_BIN/arm-unknown-nto-qnx6.5.0eabi-gcc}
RE=${QNX_READELF:-$QNX_BIN/arm-unknown-nto-qnx6.5.0eabi-readelf}
command -v "$CC" >/dev/null || { echo "BLOCKED QNX compiler: $CC"; exit 30; }
command -v "$RE" >/dev/null || { echo "BLOCKED QNX readelf: $RE"; exit 31; }
mkdir -p build/qnx
FLAGS=(-std=c99 -D_QNX_SOURCE=1 -O2 -Wall -Wextra -Werror -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp)
"$CC" "${FLAGS[@]}" tests/r143_smoke.c -lc -o build/qnx/btstack
SMOKE_SHA=$(sha256sum build/qnx/btstack | awk '{print $1}')
SMOKE_DEFINE="-DAU57X_R143_STOCK_SHA256=\\\"$SMOKE_SHA\\\""
"$CC" "${FLAGS[@]}" -fPIC -shared -Wl,--hash-style=sysv -Wl,--no-undefined \
  -DAU57X_R143_QNX_TEST=1 -DAU57X_R143_TEST_API=1 "$SMOKE_DEFINE" \
  r143_preload_probe.c bt_thread_owner.c sha256.c -lc \
  -o build/qnx/libau57x_r143_preload_REVIEW.so
"$CC" "${FLAGS[@]}" -fPIC -shared -Wl,--hash-style=sysv -Wl,--no-undefined \
  -DAU57X_R143_STOCK_SHA256=\\\"51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566\\\" \
  r143_preload_probe.c bt_thread_owner.c sha256.c -lc \
  -o build/qnx/libau57x_r143_preload_OEM_REVIEW.so
"$RE" -h -d -Ws build/qnx/libau57x_r143_preload_REVIEW.so > build/qnx/r143_smoke_elf.txt
"$RE" -h -d -Ws build/qnx/libau57x_r143_preload_OEM_REVIEW.so > build/qnx/r143_oem_elf.txt
for f in build/qnx/r143_smoke_elf.txt build/qnx/r143_oem_elf.txt; do
  grep -Eq 'Machine:.*ARM' "$f"
  grep -Fq 'Shared library: [libc.so.3]' "$f"
  grep -Eq 'FUNC[[:space:]]+GLOBAL.*pthread_create$' "$f"
  grep -Eq 'FUNC[[:space:]]+GLOBAL.*pthread_setname_np$' "$f"
done
sha256sum build/qnx/btstack build/qnx/libau57x_r143_preload_REVIEW.so build/qnx/libau57x_r143_preload_OEM_REVIEW.so > build/qnx/R143_QNX_SHA256.txt
printf 'R143_QNX_ARM32_BUILD_PASS; SYNTHETIC_RUNTIME_PENDING; OEM_PROCESS_NOT_EXECUTED; NO_INSTALL\n'
