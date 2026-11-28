#!/bin/sh
# Shared config commit/rollback primitive. Currently restricted to disposable
# host fixture. Final vehicle integration must pin the actual live config path
# and use original permissions, mount/boot transaction and MIB recovery.
set -eu
umask 077
die(){ echo "STOCK_ROUTE_FAILED: $*" >&2; exit 1; }
[ "$#" -eq 4 ] || die "usage: stock_config_apply.sh apply|restore FIXTURE_ROOT CONFIG_STAGE ORIGINAL_IAP2_SHA"
action=$1
root=$2
stage=$3
iap_pin=$4
case "$action" in apply|restore) ;; *) die "unknown action";; esac
[ -d "$root" ] && [ -f "$root/LAB_ONLY" ] && [ ! -L "$root/LAB_ONLY" ] || die "host fixture only; no vehicle activation"
case "$stage" in "$root"/stage-*) ;; *) die "stage outside fixture";; esac
case "$root:$stage/" in *'/../'*|*'/./'*|*'//'*) die "unsafe path";; esac
src="$root/original/connectivity.json"
iap="$root/original/iap2.cfg"
[ ! -L "$root/original" ] && [ ! -L "$stage" ] || die "linked directory"
hash(){ sha256sum "$1" | awk '{print $1}'; }
for f in "$src" "$iap" "$stage/original.json" "$stage/candidate.json" "$stage/SHA256.tsv" "$stage/COMPLETE"; do
 [ -f "$f" ] && [ ! -L "$f" ] || die "missing regular file"
done
[ "$(cat "$stage/COMPLETE")" = CONFIG_STAGED_NO_ACTIVATION ] || die "incomplete stage"
awk -F '\t' 'NF!=2 || length($1)!=64 || $1~/[^0123456789abcdef]/ || ($2!="original.json" && $2!="candidate.json"){bad=1} {seen[$2]++} END{exit (bad || NR!=2 || seen["original.json"]!=1 || seen["candidate.json"]!=1)}' "$stage/SHA256.tsv" || die "invalid manifest"
orig=$(awk -F '\t' '$2=="original.json"{print $1}' "$stage/SHA256.tsv")
candidate=$(awk -F '\t' '$2=="candidate.json"{print $1}' "$stage/SHA256.tsv")
[ "$(hash "$stage/original.json")" = "$orig" ] && [ "$(hash "$stage/candidate.json")" = "$candidate" ] || die "stage digest mismatch"
[ "$(hash "$iap")" = "$iap_pin" ] || die "original iAP2 config differs"
# Only the original audited enableIap field may differ. Preserve all other
# bytes, including CRLF, existing pairing and USB driver configuration.
awk 'index($0,"enableIap"){n++;if(NR==216 && $0~/^[ \t]*"enableIap"[ \t]*:[ \t]*false[ \t]*,?[ \t\r]*$/)ok=1}END{exit !(n==1&&ok)}' "$stage/original.json" || die "not expected config"
sed '216s/\("enableIap"[[:blank:]]*:[[:blank:]]*\)false/\1true/' "$stage/original.json" | cmp - "$stage/candidate.json" >/dev/null || die "unexpected change"
case "$action" in
 apply) before=$orig; after=$candidate; payload="$stage/candidate.json";;
 restore) before=$candidate; after=$orig; payload="$stage/original.json";;
esac
lock="$root/original/.mu1005-config-lock"
mkdir "$lock" || die "another transaction"
tmp="$lock/replacement"
cleanup(){ rm -f "$tmp"; rmdir "$lock" 2>/dev/null || :; }
trap cleanup 0
trap 'exit 1' 1 2 15
[ "$(hash "$src")" = "$before" ] || die "live configuration changed"
cp -p "$src" "$tmp"
cat "$payload" > "$tmp"
[ "$(hash "$tmp")" = "$after" ] || die "replacement invalid"
[ "$(hash "$iap")" = "$iap_pin" ] && [ "$(hash "$src")" = "$before" ] || die "original changed during transaction"
sync
mv -f "$tmp" "$src"
sync
[ "$(hash "$src")" = "$after" ] && [ "$(hash "$iap")" = "$iap_pin" ] || die "verify failed; retain original stage for recovery"
echo "STOCK_ROUTE_CONFIG_$action VERIFIED_HOST_ONLY"
