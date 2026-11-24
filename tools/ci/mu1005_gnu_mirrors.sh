#!/usr/bin/env bash
# Download only the original, hash-pinned GNU tarballs; no compiler source changes.
set -euo pipefail
cache=${1:?cache directory}
toolchain=${2:?toolchain directory}
mkdir -p "$cache" "$toolchain/ci-gnu"
fetch_verified() {
    local path=$1 out=$2 sha=$3 file="$cache/$2" tmp="$cache/$2.part"
    if [[ -f "$file" ]] && printf '%s  %s\n' "$sha" "$file" | sha256sum -c -; then
        printf 'Verified cached archive: %s\n' "$out"
    else
        rm -f "$file" "$tmp"
        local mirror ok=0
        for mirror in https://mirrors.kernel.org/gnu https://ftpmirror.gnu.org https://mirrors.ocf.berkeley.edu/gnu https://ftp.gnu.org/gnu; do
            printf 'Trying %s/%s\n' "$mirror" "$path"
            if curl --fail --location --silent --show-error --proto '=https' --proto-redir '=https' --connect-timeout 10 --max-time 180 "$mirror/$path" -o "$tmp" &&
               printf '%s  %s\n' "$sha" "$tmp" | sha256sum -c -; then
                mv "$tmp" "$file"
                ok=1
                break
            fi
            rm -f "$tmp"
        done
        [[ "$ok" == 1 ]] || { echo "All mirrors failed or SHA-256 mismatch: $out" >&2; return 1; }
    fi
    cp "$file" "$toolchain/ci-gnu/$out"
    printf '%s  %s\n' "$sha" "$toolchain/ci-gnu/$out" | sha256sum -c -
}
fetch_verified gcc/gcc-8.5.0/gcc-8.5.0.tar.xz gcc.tar.xz d308841a511bb830a6100397b0042db24ce11f642dab6ea6ee44842e5325ed50
fetch_verified binutils/binutils-2.38.tar.xz binutils.tar.xz e316477a914f567eccc34d5d29785b8b0f5a10208d36bbacedcc39048ecfe024
# Use the exact upstream Dockerfile. Change only the two transport lines;
# keep its original ARG SHA-256 and verification/build commands untouched.
python3 - "$toolchain/Dockerfile" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1])
s=p.read_text()
for name,url in [
    ('gcc','https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VER}/gcc-${GCC_VER}.tar.xz'),
    ('binutils','https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VER}.tar.xz'),
]:
    old='RUN curl -fsSL "'+url+'" -o /tmp/'+name+'.tar.xz && '+chr(92)+'\n    echo'
    new='COPY ci-gnu/'+name+'.tar.xz /tmp/'+name+'.tar.xz\nRUN echo'
    if s.count(old)!=1:
        raise SystemExit('Unexpected pinned Dockerfile: '+name)
    s=s.replace(old,new,1)
p.write_text(s)
PY
printf '%s\n' 'GNU_ARCHIVES_VERIFIED_ORIGINAL_SHA256_NO_SOURCE_CHANGES'
