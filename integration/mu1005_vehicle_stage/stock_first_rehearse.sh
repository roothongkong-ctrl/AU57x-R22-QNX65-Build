#!/bin/sh
# Combined original-config-first rehearsal; no production writes.
set -eu
umask 077
[ "$#" -eq 4 ] || exit 2
root=$1; config=$2; iap=$3; core=$4
[ -d "$root" ] && [ -f "$root/LAB_ONLY" ] || exit 2
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd -P)
hash(){ sha256sum "$1" | awk '{print $1}'; }
orig=56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470
cand=75671e53af6dddcba7df62f26e4e1e6ec62e1097d92b836f1df9c184742249b5
bt=51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566
[ "$(hash "$config")" = "$orig" ] && [ "$(hash "$core/btstack")" = "$bt" ]
mkdir -p "$root/original" "$root/eso/bin/apps"
cp -p "$config" "$root/original/connectivity.json"
cp -p "$iap" "$root/original/iap2.cfg"
iap_pin=$(hash "$iap")
for n in btstack bluetooth smartphone_integrator dio_manager; do
 cp -p "$core/$n" "$root/eso/bin/apps/$n"
done
sh "$here/backup.sh" host-fixture "$root/backup-one" "$root" "$bt"
sh "$here/recover.sh" verify "$root/backup-one" "$root"
sh "$here/config_transaction.sh" host-fixture "$root/original/connectivity.json" "$root/stage-one" "$root" "$orig"
[ "$(hash "$root/stage-one/candidate.json")" = "$cand" ]
before=$(stat -c '%a:%u:%g' "$root/original/connectivity.json")
sh "$here/stock_config_apply.sh" apply "$root" "$root/stage-one" "$iap_pin"
[ "$(hash "$root/original/connectivity.json")" = "$cand" ]
[ "$(hash "$root/original/iap2.cfg")" = "$iap_pin" ]
sh "$here/stock_config_apply.sh" restore "$root" "$root/stage-one" "$iap_pin"
cmp "$config" "$root/original/connectivity.json"
[ "$(stat -c '%a:%u:%g' "$root/original/connectivity.json")" = "$before" ]
sh "$here/recover.sh" stage "$root/backup-one" "$root"
for n in btstack bluetooth smartphone_integrator dio_manager; do
 cmp "$core/$n" "$root/eso/bin/apps/$n"
 cmp "$core/$n" "$root/MU1005-recovery-ready/$n"
done
cmp "$iap" "$root/original/iap2.cfg"
echo EXACT_OEM_STOCK_FIRST_TRANSACTION_AND_RECOVERY_PASS_NO_VEHICLE
