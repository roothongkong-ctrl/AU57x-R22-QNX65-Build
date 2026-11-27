#!/usr/bin/env bash
set -euo pipefail
out=qnx-runtime-evidence
mkdir -p "$out"
rc=0
docker run --rm -i --platform linux/amd64 --entrypoint /bin/bash qnx65-armv7-toolchain:8.5 -s >"$out/sdk-link.log" 2>&1 <<'SDK' || rc=$?
set -euxo pipefail
echo SDK_SCRIPT_ENTERED
cc=arm-unknown-nto-qnx6.5.0eabi-gcc
ar=arm-unknown-nto-qnx6.5.0eabi-ar
nm=arm-unknown-nto-qnx6.5.0eabi-nm
archive=/opt/qnx650/target/qnx6/armle-v7/lib/libc.a
test -s "$archive" || archive=$("$cc" -print-file-name=libc.a)
test -s "$archive"
echo "SDK_LIBC_ARCHIVE=$archive"
"$nm" -A --defined-only "$archive" 2>/dev/null | awk '$NF=="slogf" && $(NF-1) ~ /^[TW]$/ {print}' >/tmp/slogf-defs
cat /tmp/slogf-defs
test "$(wc -l </tmp/slogf-defs)" -eq 1
member=$(awk -F: '{print $(NF-1)}' /tmp/slogf-defs)
test -n "$member"
"$ar" p "$archive" "$member" >/tmp/slogf-real.o
test -s /tmp/slogf-real.o
"$nm" --defined-only /tmp/slogf-real.o | awk '$NF=="slogf" && $(NF-1) ~ /^[TW]$/ {found=1} END{exit !found}'
"$cc" -shared -Wl,--no-undefined -Wl,-soname,liblab-slogf.so /tmp/slogf-real.o -lc -o /tmp/liblab-slogf.so
"$nm" -D --defined-only /tmp/liblab-slogf.so | awk '$NF=="slogf" && $(NF-1) ~ /^[TW]$/ {found=1} END{exit !found}'
echo REAL_SDK_SLOGF_DIRECT_MEMBER_LINK_OK
SDK
cat "$out/sdk-link.log"
printf 'SDK_DIRECT_MEMBER_EXIT=%s\n' "$rc" >"$out/sdk-result.txt"
if [ "$rc" -eq 0 ] && grep -qx SDK_SCRIPT_ENTERED "$out/sdk-link.log" && grep -qx REAL_SDK_SLOGF_DIRECT_MEMBER_LINK_OK "$out/sdk-link.log";then
  echo REAL_SDK_SLOGF_DIRECT_MEMBER_LINK_OK >>"$out/sdk-result.txt"
else
  echo SDK_DIRECT_MEMBER_LINK_BLOCKED >>"$out/sdk-result.txt"
  exit 1
fi
