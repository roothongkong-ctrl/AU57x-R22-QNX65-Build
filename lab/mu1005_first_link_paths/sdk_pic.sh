#!/usr/bin/env bash
set -euo pipefail
out=qnx-runtime-evidence
mkdir -p "$out"
rc=0
docker run --rm -i --platform linux/amd64 --entrypoint /bin/bash qnx65-armv7-toolchain:8.5 -s >"$out/sdk-link.log" 2>&1 <<'SDK' || rc=$?
set -euo pipefail
echo SDK_PIC_SEARCH_ENTERED
cc=arm-unknown-nto-qnx6.5.0eabi-gcc
ar=arm-unknown-nto-qnx6.5.0eabi-ar
nm=arm-unknown-nto-qnx6.5.0eabi-nm
re=arm-unknown-nto-qnx6.5.0eabi-readelf
base=/opt/qnx650/target/qnx6/armle-v7
test -d "$base"
original=/opt/qnx650/target/qnx6/armle-v7/lib/libc.a
n=0
while IFS= read -r -d '' f; do
  if [[ "$f" -ef "$original" ]] || cmp -s "$f" "$original";then continue;fi
  if [[ "$f" == *.a ]];then
    defs=$("$nm" -A --defined-only "$f" 2>/dev/null | awk '$NF=="slogf" && $(NF-1) ~ /^[TW]$/ {print}' || true)
    test -n "$defs" || continue
    echo "ALTERNATIVE_ARCHIVE $f"
    while IFS= read -r row;do
      member=$(printf '%s\n' "$row" | awk -F: '{print $(NF-1)}')
      test -n "$member" || continue
      "$ar" p "$f" "$member" >/tmp/slog-alt.o || continue
      if "$re" -rW /tmp/slog-alt.o | grep -E 'R_ARM_MOV[WT]_ABS';then echo NON_PIC_REJECTED;continue;fi
      if "$cc" -shared -Wl,--no-undefined -Wl,-soname,liblab-slogf.so /tmp/slog-alt.o -lc -o /tmp/liblab-slogf.so &&
         "$nm" -D --defined-only /tmp/liblab-slogf.so | awk '$NF=="slogf" && $(NF-1) ~ /^[TW]$/ {found=1} END{exit !found}';then
        echo SDK_DISTINCT_PIC_LINK_OK;exit 0
      fi
    done <<< "$defs"
  else
    if "$nm" -D --defined-only "$f" 2>/dev/null | awk '$NF=="slogf" && $(NF-1) ~ /^[TW]$/ {found=1} END{exit !found}';then
      echo "SDK_EXISTING_DYNAMIC_PROVIDER $f";n=$((n+1))
    fi
  fi
done < <(find -L "$base" -type f \( -name '*.a' -o -name '*.so*' \) -print0)
if [ "$n" -gt 0 ];then echo SDK_EXISTING_DYNAMIC_PROVIDER_FOUND;exit 0;fi
echo SDK_NO_DISTINCT_PIC_PROVIDER
exit 3
SDK
cat "$out/sdk-link.log"
printf 'SDK_PIC_EXIT=%s\n' "$rc" >"$out/sdk-result.txt"
grep -E '^SDK_(DISTINCT_PIC_LINK_OK|EXISTING_DYNAMIC_PROVIDER_FOUND|NO_DISTINCT_PIC_PROVIDER)$' "$out/sdk-link.log" >>"$out/sdk-result.txt" || true
exit "$rc"
