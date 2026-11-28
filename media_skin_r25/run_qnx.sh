#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(pwd)"
UP=/tmp/qnx65-toolchain
OUT="$ROOT/media_r25_result"
mkdir -p "$OUT"
docker tag au57x-qnx65-cc:8.5 qnx65-armv7-toolchain:8.5
docker run --rm --platform linux/amd64 -v "$ROOT":/src -w /src au57x-qnx65-cc:8.5 sh -c 'arm-unknown-nto-qnx6.5.0eabi-gcc -std=c99 -O2 -Wall -Wextra -Werror -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -o media_r25_result/music_crc media_skin_r25/music_crc.c'
printf 123456789 > "$OUT/music_r25_vector"
cp media_skin_r25/music_r25_test.sh "$OUT/"
python3 - "$UP/tests/qemu-run.sh" "$UP/tests/qemu-run-music.sh" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
a='cp "$BIN" "$WORK/$NAME"\n'
assert s.count(a)==1
s=s.replace(a,a+'cp "$MUSIC_FIXTURES/music_r25_test.sh" "$MUSIC_FIXTURES/music_r25_vector" "$WORK/"\n')
a='\n$NAME\nEOF\n'
assert s.count(a)==1
s=s.replace(a,'\n$NAME\nsh\nmusic_r25_test.sh\nmusic_r25_vector\n[type=link] /bin/sh=/proc/boot/sh\nEOF\n')
Path(sys.argv[2]).write_text(s)
PY
chmod +x "$UP/tests/qemu-run-music.sh"
MUSIC_FIXTURES="$OUT" QEMU_RUN_SECS=25 QEMU_TIMEOUT=90 QEMU_KEEP_LOG="$OUT/qnx_boot.log" "$UP/tests/qemu-run-music.sh" "$OUT/music_crc" 8.5 | tee "$OUT/console.txt"
grep -Fq MUSICSKIN_NATIVE_CRC_OK "$OUT/console.txt"
grep -Fq MUSICSKIN_CRC32_SELFTEST_OK "$OUT/console.txt"
grep -Fq MUSICSKIN_QNX_SHELL_OK "$OUT/console.txt"
grep -Fq MUSICSKIN_QNX_TEST_OK "$OUT/console.txt"
