#!/bin/sh
# Explicit first-connection experiment, not an installer. Never restart or remount.
# Use only with confirmed live MU1005 paths. All probe output is allowlisted.
set -eu
umask 077
LC_ALL=C;export LC_ALL
ORIGINAL=56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470
CANDIDATE=75671e53af6dddcba7df62f26e4e1e6ec62e1097d92b836f1df9c184742249b5
HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd -P)
TXN="$HERE/../mu1005_first_link_config/first_link_config.sh"
die(){ echo "TRIAL_STOP $*" >&2;exit 2; }
digest(){
 if command -v sha256sum >/dev/null 2>&1;then sha256sum "$1" | awk '{print $1}'
 elif command -v openssl >/dev/null 2>&1;then openssl dgst -sha256 "$1" | awk '{print $NF}'
 else die NO_SHA256;fi
}
resolve_config(){
 [ -f "$1" ] && [ ! -L "$1" ] || die CONFIG_NOT_REGULAR
 d=$(CDPATH= cd -- "$(dirname "$1")" && pwd -P)
 printf '%s/connectivity.json\n' "$d"
}
# R106 real pidin ar: PID and executable are columns 1/2. The mem
# view truncates names, and grep/argument text is not a process identity.
# Query once, preserve no raw arguments. Failed/unknown enumeration stops.
snapshot_processes(){
 raw=$(pidin ar 2>/dev/null) || die PROCESS_QUERY_FAILED
 processes=$(printf '%s\n' "$raw" | awk '
  NF==0{next}
  $1=="pid" && $2=="Arguments"{header++;next}
  $1 !~ /^[0-9]+$/ || NF<2 {bad=1;next}
  {if($1==1)kernel=1;x=$2;sub(/^.*\//,"",x)
   if(x=="btstack"||x=="bluetooth"||x=="smartphone_integrator"||x=="mm-ipod"||x=="dio_manager") print x}
  END{if(bad||header!=1||!kernel)exit 2}
 ') || die PROCESS_QUERY_FORMAT_UNKNOWN
 unset raw
}
process_seen(){
 printf '%s\n' "$processes" | grep -Fxq "$1"
}
observe(){
 for name in btstack bluetooth smartphone_integrator mm-ipod dio_manager;do
   if process_seen "$name";then echo "PROCESS_PRESENT $name";else echo "PROCESS_NOT_OBSERVED $name";fi
 done
 # No raw process data, phone identity, passwords or network payload.
 # A process or device node does not establish a real session.
 if [ -e /dev/ipod0 ];then echo "NODE_PRESENT /dev/ipod0";fi
 echo "OBSERVATION_ONLY_NOT_AUTHENTICATED_SESSION"
}
[ "$#" -ge 1 ] || die USAGE
action=$1
if [ "$action" = observe ];then
 [ "$#" -eq 1 ] || die USAGE
 command -v pidin >/dev/null 2>&1 || die PIDIN_UNAVAILABLE
 snapshot_processes
 observe;exit 0
fi
[ "$#" -eq 5 ] || die 'usage: prepare|apply|restore TARGET ACTIVE_ALIAS BACKUP CONFIRM_EXPERIMENT'
target=$2;alias=$3;backup=$4
[ "$5" = CONFIRM_EXPERIMENT ] || die CONFIRMATION_REQUIRED
case "$target" in /*/connectivity.json);;*)die TARGET_PATH;;esac
case "$alias" in /*/connectivity.json);;*)die ALIAS_PATH;;esac
case "$backup" in /*);;*)die BACKUP_PATH;;esac
[ "$(resolve_config "$target")" = "$target" ] || die TARGET_NOT_CANONICAL
[ "$(resolve_config "$alias")" = "$target" ] || die ALIAS_NOT_SAME_CANONICAL_FILE
hash=$(digest "$target")
if [ "$action" = restore ];then
 sh "$TXN" restore "$target" "$backup" CONFIRM_EXPERIMENT
 echo "TRIAL_RESTORED_NOT_CONNECTION";exit 0
fi
[ "$hash" = "$ORIGINAL" ] || die CONFIG_NOT_EXACT_ORIGINAL
command -v pidin >/dev/null 2>&1 || die PIDIN_UNAVAILABLE
snapshot_processes
for name in btstack bluetooth smartphone_integrator;do
 process_seen "$name" || die "OEM_PROCESS_NOT_OBSERVED_$name"
done
# Avoid applying while original media/phone session processes are observed.
# This is conservative; no assumption that their absence proves no wired session.
for name in mm-ipod dio_manager;do
 if process_seen "$name";then die "MEDIA_PROCESS_PRESENT_$name";fi
done
case "$action" in
prepare)echo "TRIAL_PREPARED_NO_WRITE";;
apply)
 sh "$TXN" apply "$target" "$backup" CONFIRM_EXPERIMENT
 [ "$(digest "$target")" = "$CANDIDATE" ] || die CANDIDATE_VERIFY_FAILED
 echo "TRIAL_CONFIG_APPLIED_RESTART_NOT_PERFORMED"
 echo "TRIAL_NOT_CONNECTION_PROOF";;
*)die UNKNOWN_ACTION;;
esac
