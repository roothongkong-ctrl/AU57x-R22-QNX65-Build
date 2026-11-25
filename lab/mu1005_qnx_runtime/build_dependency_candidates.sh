#!/usr/bin/env bash
# Build only an upstream ICU data candidate. Never substitute for stock on car.
set -euxo pipefail
cd "$(dirname "$0")/../.."
e="$PWD/qnx-runtime-evidence/stock-dependency-candidates"
mkdir -p "$e"
url=https://github.com/unicode-org/icu/releases/download/release-58-2
curl --fail --location --retry 3 "$url/icu4c-58_2-src.tgz" -o "$e/icu4c-58_2-src.tgz"
curl --fail --location --retry 3 "$url/icu4c-src-58_2.md5" -o "$e/upstream.md5"
(cd "$e" && grep -E '^[0-9a-fA-F]{32} [ *]icu4c-58_2-src\.tgz$' upstream.md5 | md5sum -c -)
sha256sum "$e/icu4c-58_2-src.tgz" > "$e/source.sha256"
python3 - "$e" <<'PY'
import sys,tarfile,pathlib
p=pathlib.Path(sys.argv[1])
with tarfile.open(p/'icu4c-58_2-src.tgz') as t:
    candidates=[m for m in t.getmembers() if m.isfile() and m.name.endswith('/data/in/icudt58l.dat')]
    assert len(candidates)==1, [m.name for m in candidates]
    d=t.extractfile(candidates[0]).read()
assert 1000000<len(d)<100000000
assert d[2:4]==b'\xda\x27' and d[8]==0 and d[12:16]==b'CmnD', d[:24].hex()
(p/'icudt58l.dat').write_bytes(d)
print('UPSTREAM_ICU58_LITTLE_ENDIAN_DATA',len(d),d[:24].hex())
PY
cat > "$e/icudata.S" <<'ASM'
.section .rodata
.balign 16
.global icudt58_dat
.type icudt58_dat,%object
icudt58_dat:
.incbin "icudt58l.dat"
.size icudt58_dat, .-icudt58_dat
.section .note.GNU-stack,"",%progbits
ASM
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src/qnx-runtime-evidence/stock-dependency-candidates qnx65-armv7-toolchain:8.5 bash -c '
set -eux
C=arm-unknown-nto-qnx6.5.0eabi-gcc
R=arm-unknown-nto-qnx6.5.0eabi-readelf
"$C" -shared -nostdlib -Wl,-soname,libicudata.so.58 -Wl,--no-undefined icudata.S -o libicudata.so.58
"$R" -h -dW -sW libicudata.so.58 > icudata-elf.log
grep -E "Machine:.*ARM" icudata-elf.log
grep -F "[libicudata.so.58]" icudata-elf.log
awk -v size="$(wc -c < icudt58l.dat)" '\''$4=="OBJECT" && $5=="GLOBAL" && $7!="UND" && $8=="icudt58_dat" && $3==size {ok=1} END{exit !ok}'\'' icudata-elf.log
! grep -q "(NEEDED)" icudata-elf.log
'
# XML is an existing SDK candidate, not an OEM match. Require actual four exports.
xml=toolchain/sdp/target/qnx6/armle-v7/usr/lib/libxml2.so.1
readelf -h -dW -D -sW "$xml" > "$e/xml-elf.log"
grep -E 'Machine:.*ARM' "$e/xml-elf.log"
for name in xmlCreatePushParserCtxt xmlCtxtUseOptions xmlFreeParserCtxt xmlParseChunk; do
  awk -v n="$name" '$7!="UND" && $8==n {ok=1} END{exit !ok}' "$e/xml-elf.log"
done
sha256sum "$xml" "$e/libicudata.so.58" "$e/icudt58l.dat" > "$e/candidate.sha256"
rm "$e/icu4c-58_2-src.tgz" "$e/icudt58l.dat"
printf '%s\n' 'Candidate ICU upstream data and SDK XML export checks passed; not OEM, not runtime verified, no install.' > "$e/STATUS.txt"
