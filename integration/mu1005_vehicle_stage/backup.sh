#!/bin/sh
# MU1005 backup stage only. No original-file writes, service changes or flash.
# Run manually only after final package review; no automatic execution.
set -eu
umask 077
fail() { echo "BACKUP_FAILED: $*" >&2; exit 1; }
mode=${1:-}
dest=${2:-}
case "$mode" in
  vehicle)
    root=
    expected=51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566
    case "$dest" in /fs/sda0/*|/fs/sdb0/*) ;; *) fail "select a mounted SD-card directory explicitly";; esac
    [ -d "${dest%/*}" ] || fail "SD parent unavailable"
    ;;
  host-fixture)
    root=${3:-}; expected=${4:-}
    [ -n "$root" ] && [ -f "$root/LAB_ONLY" ] || fail "fixture marker missing"
    case "$dest" in "$root"/backup-*) ;; *) fail "fixture destination outside lab";; esac
    ;;
  *) fail "usage: backup.sh vehicle /fs/sda0/backup-MU1005";;
esac
case "$dest/" in *'/../'*|*'/./'*|*'//'*) fail "unsafe destination";; esac
hash() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'
  elif command -v openssl >/dev/null 2>&1; then openssl dgst -sha256 "$1" | awk '{print $NF}'
  else fail "no SHA256 utility"; fi
}
base="$root/eso/bin/apps"
[ -f "$base/btstack" ] || base="$root/mnt/app/eso/bin/apps"
[ -f "$base/btstack" ] || fail "stock btstack not found"
[ "$(hash "$base/btstack")" = "$expected" ] || fail "wrong stock btstack hash"
[ ! -e "$dest" ] && [ ! -L "$dest" ] || fail "backup exists; never overwrite"
mkdir "$dest" || fail "cannot create backup"
trap 'echo BACKUP_INCOMPLETE >&2' 0
: > "$dest/SHA256.tsv"
for n in btstack bluetooth smartphone_integrator dio_manager; do
  src="$base/$n"
  [ -f "$src" ] || fail "required original missing: $n"
  digest=$(hash "$src")
  cp -p "$src" "$dest/$n" || fail "copy failed: $n"
  [ "$(hash "$dest/$n")" = "$digest" ] || fail "copy mismatch: $n"
  [ "$(hash "$src")" = "$digest" ] || fail "original changed during backup: $n"
  printf '%s\t%s\t%s\n' "$digest" "$n" "$src" >> "$dest/SHA256.tsv"
done
sync
printf '%s\n' 'BACKUP_VERIFIED_CORE_ONLY' > "$dest/COMPLETE"
sync
trap - 0
echo "BACKUP_VERIFIED_CORE_ONLY: $dest"
