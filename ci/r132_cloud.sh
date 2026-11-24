#!/bin/bash
set -Eeuo pipefail
mkdir -p build/qnx
sha256sum -c R132_TRACE_SHA256.txt | tee build/qnx/r132_source_hashes.log
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
pin=baa45224f18ae6b13c2e9c77c6663bb8181eb6a1
git init -q "$tmp/toolchain"
git -C "$tmp/toolchain" remote add origin https://github.com/luka-dev/qnx65-armv7-toolchain.git
git -C "$tmp/toolchain" fetch --depth 1 origin "$pin"
git -C "$tmp/toolchain" checkout -q FETCH_HEAD
git -C "$tmp/toolchain" rev-parse HEAD >build/qnx/toolchain_commit.txt
if [ "$(docker image inspect --format '{{index .Config.Labels "org.au57x.toolchain-revision"}}' qnx65-armv7-toolchain:8.5 2>/dev/null || true)" != "$pin" ]; then
 docker build --platform linux/amd64 --target base-env --build-arg BASE=base-8.5 --label "org.au57x.toolchain-revision=$pin" -t qnx65-armv7-toolchain:8.5 "$tmp/toolchain" 2>&1 | tee build/qnx/r132_toolchain.log
fi
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src qnx65-armv7-toolchain:8.5 bash /src/ci/r132_target.sh 2>&1 | tee build/qnx/r132_target.log
QEMU_RUN_SECS=20 QEMU_TIMEOUT=80 QEMU_KEEP_LOG="$PWD/build/qnx/r132_boot.log" bash "$tmp/toolchain/tests/qemu-run.sh" "$PWD/build/qnx/r132_arm_trace_smoke" 8.5 2>&1 | tee build/qnx/r132_runtime.log
grep -Fq AU57X_R132_QNX_ARM_VARIADIC_PASSTHROUGH_OK_NO_OEM build/qnx/r132_runtime.log
