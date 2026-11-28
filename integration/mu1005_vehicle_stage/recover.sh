#!/bin/sh
# Restore verified originals to a NEW recovery staging directory only.
# No OEM file replacement, flash, boot-hook or process changes.
set -eu
umask 077
fail() { echo "RECOVERY_FAILED: $*" >&2; exit 1; }
[ "$#" -eq 3 ] || fail "usage: recover.sh verify|stage BACKUP RECOVERY_PARENT"
mode=$1
backup=$2
parent=$3
case "$mode" in verify|stage) ;; *) fail "unknown action";; esac
[ -d "$backup" ] && [ ! -L "$backup" ] || fail "backup unavailable"
[ -f "$backup/COMPLETE" ] && [ ! -L "$backup/COMPLETE" ] || fail "incomplete backup"
[ "$(cat "$backup/COMPLETE")" = BACKUP_VERIFIED_CORE_ONLY ] || fail "bad completion marker"
[ -f "$backup/SHA256.tsv" ] && [ ! -L "$backup/SHA256.tsv" ] || fail "missing manifest"
hash() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'
  elif command -v openssl >/dev/null 2>&1; then openssl dgst -sha256 "$1" | awk '{print $NF}'
  else fail "no SHA256 utility"; fi
}
# Strict inventory: four fixed names, exactly one entry per name, no other paths.
awk -F '\t' '
  NF!=3 || length($1)!=64 || $1 ~ /[^0123456789abcdef]/ {bad=1}
  $2!="btstack" && $2!="bluetooth" && $2!="smartphone_integrator" && $2!="dio_manager" {bad=1}
  {seen[$2]++}
  END {if(NR!=4 || seen["btstack"]!=1 || seen["bluetooth"]!=1 || seen["smartphone_integrator"]!=1 || seen["dio_manager"]!=1 || bad) exit 1}
' "$backup/SHA256.tsv" || fail "invalid inventory"
for name in btstack bluetooth smartphone_integrator dio_manager; do
  expected=$(awk -F '\t' -v n="$name" '$2==n {print $1}' "$backup/SHA256.tsv")
  [ -f "$backup/$name" ] && [ ! -L "$backup/$name" ] || fail "missing original $name"
  [ "$(hash "$backup/$name")" = "$expected" ] || fail "corrupt backup $name"
done
if [ "$mode" = verify ]; then echo "RECOVERY_BACKUP_VERIFIED_CORE_ONLY"; exit 0; fi
[ -d "$parent" ] && [ ! -L "$parent" ] || fail "recovery parent unavailable"
# Only removable SD paths or an explicitly marked disposable host fixture.
case "$parent" in
  /fs/sda0/*|/fs/sdb0/*) ;;
  *) [ -f "$parent/LAB_ONLY" ] || fail "not an authorized staging parent";;
esac
case "$parent/" in *'/../'*|*'/./'*|*'//'*) fail "unsafe staging parent";; esac
final="$parent/MU1005-recovery-ready"
[ ! -e "$final" ] && [ ! -L "$final" ] || fail "previous recovery set exists"
work="$parent/.MU1005-recovery-$$"
mkdir "$work" || fail "cannot create private stage"
cleanup() { for n in btstack bluetooth smartphone_integrator dio_manager SHA256.tsv COMPLETE; do rm -f "$work/$n"; done; rmdir "$work" 2>/dev/null || :; }
trap cleanup 0
trap 'exit 1' 1 2 15
for name in btstack bluetooth smartphone_integrator dio_manager; do
  expected=$(awk -F '\t' -v n="$name" '$2==n {print $1}' "$backup/SHA256.tsv")
  cp -p "$backup/$name" "$work/$name" || fail "copy failed $name"
  [ "$(hash "$work/$name")" = "$expected" ] || fail "staging digest mismatch $name"
done
cp "$backup/SHA256.tsv" "$work/SHA256.tsv"
printf '%s\n' RECOVERY_STAGED_CORE_ONLY > "$work/COMPLETE"
sync
# POSIX mv may move into an existing directory; an exclusive lock prevents
# concurrent script invocations; destination is rechecked before publication.
lock="$parent/.MU1005-recovery-lock"
mkdir "$lock" || fail "recovery already running or lock requires inspection"
trap 'cleanup; rmdir "$lock" 2>/dev/null || :' 0
[ ! -e "$final" ] && [ ! -L "$final" ] || fail "recovery destination appeared"
mv "$work" "$final" || fail "publication failed"
sync
rmdir "$lock"
trap - 0 1 2 15
echo "RECOVERY_STAGED_CORE_ONLY: $final"
