#!/bin/sh
# Read-only parked-vehicle probe, never mounts, writes, or starts services.
# Manifest from trusted R105.185 inventory; validate package hash first.
set -u
LC_ALL=C; export LC_ALL
here=$(CDPATH= cd "$(dirname "$0")" && pwd -P) || exit 2
while read -r expected size p extra; do
 [ -z "${extra:-}" ] || exit 2
 case "$p" in /eso/bin/apps/*|/mnt/app/eso/bin/apps/*|/armle/usr/sbin/*|/mnt/app/armle/usr/sbin/*|/armle/lib/dll/*|/mnt/app/armle/lib/dll/*|/etc/eso/production/*|/mnt/system/etc/eso/production/*|/etc/mm/*|/mnt/system/etc/mm/*) ;; *) exit 2;; esac
 case "$p" in */../*|*/./*|*//*|*/..) exit 2;; esac
 [ -f "$p" ] || { printf 'NOT_FOUND %s\n' "$p"; continue; }
 if command -v sha256sum >/dev/null 2>&1; then d=$(sha256sum "$p" 2>/dev/null|awk '{print $1}')
 elif command -v openssl >/dev/null 2>&1; then d=$(openssl dgst -sha256 "$p" 2>/dev/null|awk '{print $NF}')
 else d=; fi
 if [ -z "$d" ]; then status=HASH_UNAVAILABLE
 elif [ "$d" = "$expected" ]; then status=EXACT_STOCK
 else status=DIFFERENT_BYTES; fi
 printf 'FILE %s %s\n' "$p" "$status"
done < "$here/candidate_paths.tsv"
printf 'READ_ONLY_CANDIDATES_NOT_RUNTIME_WRITE_AUTHORIZATION\n'
