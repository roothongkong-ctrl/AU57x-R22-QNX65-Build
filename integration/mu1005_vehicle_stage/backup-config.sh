#!/bin/sh
# SD backup of native configuration in addition to existing four core binaries.
# Read-only with respect to the car. Never overwrite an existing backup.
set -eu
umask 077
fail(){ echo "CONFIG_BACKUP_FAILED: $*" >&2; exit 1; }
[ "$#" -eq 2 ] || fail "usage: backup-config.sh VERIFIED_CORE_BACKUP NEW_SD_CONFIG_DIR"
core=$1
dest=$2
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
case "$dest" in /fs/sda0/*|/fs/sdb0/*) ;; *) fail "SD required";; esac
case "$dest/" in *'/../'*|*'/./'*|*'//'*) fail "unsafe path";; esac
[ ! -e "$dest" ] && [ ! -L "$dest" ] || fail "destination exists"
sh "$here/recover.sh" verify "$core" "$here"
hash(){ if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'; elif command -v openssl >/dev/null 2>&1; then openssl dgst -sha256 "$1" | awk '{print $NF}'; else fail "no SHA256 utility"; fi; }
# Select actual readable configuration, refusing distinct conflicting copies.
choose(){
  name=$1; selected=
  for f in "/mnt/system/etc/eso/production/$name" "/etc/eso/production/$name"; do
    [ -f "$f" ] || continue
    if [ -n "$selected" ]; then
      [ "$f" -ef "$selected" ] || fail "ambiguous live path $name"
    else selected=$f; fi
  done
  [ -n "$selected" ] || fail "missing $name"
  printf '%s\n' "$selected"
}
mkdir "$dest"
trap 'echo CONFIG_BACKUP_INCOMPLETE >&2' 0
: > "$dest/SHA256.tsv"
for name in connectivity.json dio_manager.json iap2.cfg; do
  src=$(choose "$name")
  old=$(hash "$src")
  cp -p "$src" "$dest/$name"
  [ "$(hash "$dest/$name")" = "$old" ] && [ "$(hash "$src")" = "$old" ] || fail "copy mismatch $name"
  printf '%s\t%s\t%s\n' "$old" "$name" "$src" >> "$dest/SHA256.tsv"
done
sync
printf '%s\n' CONFIG_BACKUP_VERIFIED > "$dest/COMPLETE"
sync
trap - 0
echo "CONFIG_BACKUP_VERIFIED: $dest"
