#!/bin/sh
# Exact original connectivity.json delta, executable on QNX without Python.
# Production mode stages to SD only. Never changes the live configuration.
set -eu
umask 077
fail(){ echo "CONFIG_TRANSACTION_FAILED: $*" >&2; exit 1; }
[ "$#" -ge 3 ] || fail "usage: config_transaction.sh vehicle|host-fixture ORIGINAL SD_STAGE [FIXTURE_ROOT EXPECTED]"
mode=$1
src=$2
dest=$3
pin=56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470
case "$mode" in
 vehicle)
   [ "$#" -eq 3 ] || fail "unexpected vehicle parameters"
   case "$dest" in /fs/sda0/*|/fs/sdb0/*) ;; *) fail "explicit SD destination required";; esac
   ;;
 host-fixture)
   [ "$#" -eq 5 ] || fail "fixture parameters required"
   root=$4; pin=$5
   [ -f "$root/LAB_ONLY" ] && [ ! -L "$root" ] || fail "fixture marker missing"
   case "$src:$dest" in "$root"/original/connectivity.json:"$root"/stage-*) ;; *) fail "fixture path mismatch";; esac
   ;;
 *) fail "unknown mode";;
esac
case "$src:$dest/" in *'/../'*|*'/./'*|*'//'*) fail "unsafe path";; esac
[ "$pin" != "" ] && [ -f "$src" ] && [ ! -L "$src" ] || fail "original missing"
hash(){
 if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'
 elif command -v openssl >/dev/null 2>&1; then openssl dgst -sha256 "$1" | awk '{print $NF}'
 else fail "SHA256 unavailable"; fi
}
[ "$(hash "$src")" = "$pin" ] || fail "unexpected original config"
[ ! -e "$dest" ] && [ ! -L "$dest" ] || fail "stage already exists"
parent=$(dirname "$dest")
[ -d "$parent" ] && [ ! -L "$parent" ] || fail "stage parent unavailable"
work="$parent/.mu1005-config-$$"
mkdir "$work" || fail "cannot create private stage"
cleanup(){ for f in original.json candidate.json reverse.json SHA256.tsv COMPLETE; do rm -f "$work/$f"; done; rmdir "$work" 2>/dev/null || :; }
trap cleanup 0
trap 'exit 1' 1 2 15
cp -p "$src" "$work/original.json"
[ "$(hash "$work/original.json")" = "$pin" ] || fail "backup mismatch"
# Match the existing exact audited delta at line 216, no other setting.
awk 'index($0,"enableIap"){n++; if(NR==216 && $0 ~ /^[ \t]*"enableIap"[ \t]*:[ \t]*false[ \t]*,?[ \t\r]*$/) ok=1} END{exit !(n==1 && ok)}' "$work/original.json" || fail "unexpected original setting"
sed '216s/\("enableIap"[[:blank:]]*:[[:blank:]]*\)false/\1true/' "$work/original.json" > "$work/candidate.json"
sed '216s/\("enableIap"[[:blank:]]*:[[:blank:]]*\)true/\1false/' "$work/candidate.json" > "$work/reverse.json"
cmp "$work/original.json" "$work/reverse.json" >/dev/null || fail "not byte-reversible"
[ "$(hash "$work/candidate.json")" != "$pin" ] || fail "setting not modified"
[ "$(hash "$src")" = "$pin" ] || fail "original changed"
printf '%s\toriginal.json\n%s\tcandidate.json\n' "$pin" "$(hash "$work/candidate.json")" > "$work/SHA256.tsv"
rm "$work/reverse.json"
printf '%s\n' CONFIG_STAGED_NO_ACTIVATION > "$work/COMPLETE"
sync
lock="$parent/.mu1005-config-lock"
mkdir "$lock" || fail "another transaction or stale lock"
trap 'cleanup; rmdir "$lock" 2>/dev/null || :' 0
[ ! -e "$dest" ] && [ ! -L "$dest" ] || fail "destination appeared"
mv "$work" "$dest"
sync
rmdir "$lock"
trap - 0 1 2 15
echo CONFIG_STAGED_NO_ACTIVATION
