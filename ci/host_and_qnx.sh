#!/bin/bash
set -Eeuo pipefail
mkdir -p build/qnx
sha256sum -c R125_SOURCE_SHA256.txt | tee build/qnx/source_hashes.log
gcc -std=c11 -O2 -Wall -Wextra -Werror -fPIC -shared -Istock_transport -Iradio_stream/src stock_transport/r123_rf_transport.c stock_transport/r124_driver_iface.c stock_transport/r125_module.c stock_transport/r129_driver_call.c radio_stream/src/wcp_wire.c -o /tmp/r125-plugin-host.so
gcc -std=c11 -O2 -Wall -Wextra -Werror -Istock_transport -Iradio_stream/src ci/r125_plugin_smoke.c -ldl -o /tmp/r125-smoke
/tmp/r125-smoke /tmp/r125-plugin-host.so | tee build/qnx/host_smoke.log
gcc -std=c11 -Wall -Wextra -Werror -rdynamic -Istock_transport -Iradio_stream/src stock_transport/r129_driver_call.c tests/test_r129_driver_call.c -ldl -o /tmp/r129-call-test
/tmp/r129-call-test | tee build/qnx/r129_host_call.log
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
git init -q "$tmp/toolchain"
git -C "$tmp/toolchain" remote add origin https://github.com/luka-dev/qnx65-armv7-toolchain.git
pin=baa45224f18ae6b13c2e9c77c6663bb8181eb6a1
# Reuse an already-built image on persistent/local runners only, after matching exact toolchain revision.
# Hosted public runners do not persist the SDK image or proprietary sysroot in Actions cache.
git -C "$tmp/toolchain" fetch --depth 1 origin "$pin"
git -C "$tmp/toolchain" checkout -q FETCH_HEAD
git -C "$tmp/toolchain" rev-parse HEAD > build/qnx/toolchain_commit.txt
if [ "$(docker image inspect --format '{{index .Config.Labels "org.au57x.toolchain-revision"}}' qnx65-armv7-toolchain:8.5 2>/dev/null || true)" = "$pin" ]; then
  echo "Reusing revision-pinned local toolchain image" | tee build/qnx/toolchain_build.log
else
  docker build --platform linux/amd64 --target base-env --build-arg BASE=base-8.5 --label "org.au57x.toolchain-revision=$pin" -t qnx65-armv7-toolchain:8.5 "$tmp/toolchain" 2>&1 | tee build/qnx/toolchain_build.log
fi
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
# Invoke via bash so execution is independent of mount permissions or shebang handling.
QNX_R125_REVIEW_SO="$PWD/build/qnx/ipod-transport-au57xbt-REVIEW.so" QEMU_RUN_SECS=20 QEMU_TIMEOUT=80 QEMU_KEEP_LOG="$PWD/build/qnx/boot_console.log" bash "$tmp/toolchain/tests/qemu-run-r125.sh" "$PWD/build/qnx/r125_plugin_smoke" 8.5 2>&1 | tee build/qnx/runtime_smoke.log
grep -Fq AU57X_R125_QNX_PLUGIN_DLOPEN_OK_NO_OEM build/qnx/runtime_smoke.log
QNX_R125_REVIEW_SO="$PWD/build/qnx/ipod-transport-au57xbt-REVIEW.so" QEMU_RUN_SECS=20 QEMU_TIMEOUT=80 QEMU_KEEP_LOG="$PWD/build/qnx/pulse_boot_console.log" bash "$tmp/toolchain/tests/qemu-run-r125.sh" "$PWD/build/qnx/r127_pulse_smoke" 8.5 2>&1 | tee build/qnx/pulse_runtime.log
grep -Fq AU57X_R127_QNX_PULSE_DEFERRED_LOADER_OK_NO_OEM build/qnx/pulse_runtime.log
