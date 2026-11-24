#!/bin/bash
set -Eeuo pipefail
mkdir -p build/qnx
sha256sum -c R137_PRELOAD_SHA256.txt
sha256sum -c R132_TRACE_SHA256.txt
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
pin=baa45224f18ae6b13c2e9c77c6663bb8181eb6a1
git init -q "$tmp/toolchain"
git -C "$tmp/toolchain" remote add origin https://github.com/luka-dev/qnx65-armv7-toolchain.git
git -C "$tmp/toolchain" fetch --depth 1 origin "$pin"
git -C "$tmp/toolchain" checkout -q FETCH_HEAD
git -C "$tmp/toolchain" rev-parse HEAD >build/qnx/toolchain_commit.txt
if [ "$(docker image inspect --format '{{index .Config.Labels "org.au57x.toolchain-revision"}}' qnx65-armv7-toolchain:8.5 2>/dev/null || true)" != "$pin" ]; then
 docker build --platform linux/amd64 --target base-env --build-arg BASE=base-8.5 --label "org.au57x.toolchain-revision=$pin" -t qnx65-armv7-toolchain:8.5 "$tmp/toolchain" 2>&1 | tee build/qnx/r137_toolchain.log
fi
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src qnx65-armv7-toolchain:8.5 bash /src/ci/r137_target.sh 2>&1 | tee build/qnx/r137_target.log
python3 - "$tmp/toolchain/tests/qemu-run.sh" "$tmp/toolchain/tests/qemu-run-r142.sh" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
pairs=[
 ('cp "$BIN" "$WORK/$NAME"\n','cp "$BIN" "$WORK/$NAME"\ncp "$R137_REVIEW_LIB" "$WORK/"\n'),
 ('EOF\n\ndocker run --rm --platform=linux/amd64 -v "$WORK":/w','EOF\nprintf "%s\\n" "$(basename "$R137_REVIEW_LIB")" >> "$WORK/test.build"\n\ndocker run --rm --platform=linux/amd64 -v "$WORK":/w'),
 ('    $NAME\n    display_msg "@@QNXRUN-END@@"','    display_msg "R142_BASELINE_BEFORE"\n    $NAME baseline\n    display_msg "R142_PRELOAD"\n    LD_PRELOAD=/proc/boot/libau57x_r137_preload_REVIEW.so $NAME preloaded\n    display_msg "R142_BASELINE_AFTER"\n    $NAME baseline\n    display_msg "@@QNXRUN-END@@"')
]
for a,b in pairs:
 if s.count(a)!=1: raise SystemExit('unexpected upstream anchor: '+a)
 s=s.replace(a,b,1)
Path(sys.argv[2]).write_text(s)
PY
R137_REVIEW_LIB="$PWD/build/qnx/libau57x_r137_preload_REVIEW.so" QEMU_RUN_SECS=25 QEMU_TIMEOUT=90 QEMU_KEEP_LOG="$PWD/build/qnx/r142_boot.log" bash "$tmp/toolchain/tests/qemu-run-r142.sh" "$PWD/build/qnx/r137_preload_test" 8.5 2>&1 | tee build/qnx/r142_runtime.log
python3 ci/r142_validate.py build/qnx/r142_boot.log | tee build/qnx/r142_verdict.log
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src qnx65-armv7-toolchain:8.5 bash /src/ci/r140_trace_target.sh 2>&1 | tee build/qnx/r132_fixed_target.log
QEMU_RUN_SECS=20 QEMU_TIMEOUT=80 QEMU_KEEP_LOG="$PWD/build/qnx/r132_fixed_boot.log" bash "$tmp/toolchain/tests/qemu-run.sh" "$PWD/build/qnx/r132_arm_trace_smoke" 8.5 2>&1 | tee build/qnx/r132_fixed_runtime.log
grep -Fq AU57X_R132_QNX_ARM_VARIADIC_PASSTHROUGH_OK_NO_OEM build/qnx/r132_fixed_runtime.log
