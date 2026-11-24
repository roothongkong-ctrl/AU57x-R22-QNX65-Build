#!/bin/bash
set -Eeuo pipefail
mkdir -p build/qnx
exec > >(tee build/qnx/native_plugin_build.log) 2>&1
set -x
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
RE=arm-unknown-nto-qnx6.5.0eabi-readelf
command -v "$CC"; command -v "$RE"
"$CC" -dM -E -x c /dev/null >build/qnx/compiler_macros.txt
grep -q '__QNXNTO__' build/qnx/compiler_macros.txt
grep -q '__arm__' build/qnx/compiler_macros.txt
grep -Eq '^#define __SIZEOF_POINTER__ 4$' build/qnx/compiler_macros.txt
flags=(-std=c11 -D_QNX_SOURCE=1 -include sys/types.h -O2 -Wall -Wextra -Werror -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -mno-unaligned-access -Istock_transport -Iradio_stream/src)
# Real target compiler, check every public header in isolation before linking.
for h in wcp_wire.h r123_rf_transport.h r124_driver_iface.h r125_module.h; do
 printf '#include "%s"\n_Static_assert(sizeof(size_t)==4,"QNX size_t");\n' "$h" | "$CC" "${flags[@]}" -x c -fsyntax-only -
done
"$CC" "${flags[@]}" -fPIC -shared -Wl,--hash-style=sysv -Wl,--no-undefined stock_transport/r123_rf_transport.c stock_transport/r124_driver_iface.c stock_transport/r125_module.c radio_stream/src/wcp_wire.c -Wl,--no-as-needed -lsocket -lc -Wl,--as-needed -o build/qnx/ipod-transport-au57xbt-REVIEW.so
"$RE" -h -d -Ws build/qnx/ipod-transport-au57xbt-REVIEW.so > build/qnx/plugin_readelf.txt
grep -Eq 'Class:.*ELF32' build/qnx/plugin_readelf.txt
grep -Eq 'Machine:.*ARM' build/qnx/plugin_readelf.txt
grep -Eq 'Type:.*DYN' build/qnx/plugin_readelf.txt
grep -Eq '36[[:space:]]+OBJECT[[:space:]]+GLOBAL.*ipod_module$' build/qnx/plugin_readelf.txt
grep -Fq 'Shared library: [libc.so.3]' build/qnx/plugin_readelf.txt
grep -Fq 'Shared library: [libsocket.so.3]' build/qnx/plugin_readelf.txt
"$CC" "${flags[@]}" ci/r125_plugin_smoke.c -o build/qnx/r125_plugin_smoke
"$RE" -h -d build/qnx/r125_plugin_smoke > build/qnx/smoke_readelf.txt
sha256sum build/qnx/ipod-transport-au57xbt-REVIEW.so build/qnx/r125_plugin_smoke > build/qnx/TARGET_SHA256SUMS.txt
echo AU57X_R125_QNX_ARM_PLUGIN_LINKED_REVIEW_ONLY
