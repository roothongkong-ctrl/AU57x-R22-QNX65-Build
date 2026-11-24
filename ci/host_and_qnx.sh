#!/bin/bash
set -Eeuo pipefail
mkdir -p build/qnx
sha256sum -c R125_SOURCE_SHA256.txt | tee build/qnx/source_hashes.log
gcc -std=c11 -O2 -Wall -Wextra -Werror -fPIC -shared -Istock_transport -Iradio_stream/src stock_transport/r123_rf_transport.c stock_transport/r124_driver_iface.c stock_transport/r125_module.c radio_stream/src/wcp_wire.c -o /tmp/r125-plugin-host.so
gcc -std=c11 -O2 -Wall -Wextra -Werror -Istock_transport -Iradio_stream/src ci/r125_plugin_smoke.c -ldl -o /tmp/r125-smoke
/tmp/r125-smoke /tmp/r125-plugin-host.so | tee build/qnx/host_smoke.log
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
git init -q "$tmp/toolchain"
git -C "$tmp/toolchain" remote add origin https://github.com/luka-dev/qnx65-armv7-toolchain.git
git -C "$tmp/toolchain" fetch --depth 1 origin baa45224f18ae6b13c2e9c77c6663bb8181eb6a1
git -C "$tmp/toolchain" checkout -q FETCH_HEAD
git -C "$tmp/toolchain" rev-parse HEAD > build/qnx/toolchain_commit.txt
docker build --platform linux/amd64 --target base-env --build-arg BASE=base-8.5 -t qnx65-armv7-toolchain:8.5 "$tmp/toolchain" 2>&1 | tee build/qnx/toolchain_build.log
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src qnx65-armv7-toolchain:8.5 bash /src/ci/target_plugin.sh
python3 - "$tmp/toolchain/tests/qemu-run.sh" "$tmp/toolchain/tests/qemu-run-r125.sh" <<'PY'
from pathlib import Path
import sys
src=Path(sys.argv[1]).read_text()
anchors=[('cp "$BIN" "$WORK/$NAME"\n','cp "$BIN" "$WORK/$NAME"\ncp "$QNX_R125_REVIEW_SO" "$WORK/"\n'),
 ('EOF\n\ndocker run --rm --platform=linux/amd64 -v "$WORK":/w','EOF\nprintf "%s\\n" "$(basename "$QNX_R125_REVIEW_SO")" >> "$WORK/test.build"\n\ndocker run --rm --platform=linux/amd64 -v "$WORK":/w')]
for before,after in anchors:
 if src.count(before)!=1:raise SystemExit('QEMU upstream script anchor changed')
 src=src.replace(before,after,1)
Path(sys.argv[2]).write_text(src)
PY
chmod +x "$tmp/toolchain/tests/qemu-run-r125.sh"
QNX_R125_REVIEW_SO="$PWD/build/qnx/ipod-transport-au57xbt-REVIEW.so" QEMU_RUN_SECS=20 QEMU_TIMEOUT=80 QEMU_KEEP_LOG="$PWD/build/qnx/boot_console.log" "$tmp/toolchain/tests/qemu-run-r125.sh" "$PWD/build/qnx/r125_plugin_smoke" 8.5 2>&1 | tee build/qnx/runtime_smoke.log
grep -Fq AU57X_R125_QNX_PLUGIN_DLOPEN_OK_NO_OEM build/qnx/runtime_smoke.log
