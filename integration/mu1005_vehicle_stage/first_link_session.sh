#!/bin/sh
# One-session original-route review. Default read-only. No automatic install,
# mount, reboot, kill, injection, or donor binary activation.
set -eu
umask 077
here=$(CDPATH= cd "$(dirname "$0")" && pwd -P)
case "$here" in /fs/sda0/*|/fs/sdb0/*|/net/mmx/fs/sda0/*) ;; *) echo SD_PATH_NOT_CONFIRMED;exit 2;; esac
trial="$here/lab/mu1005_first_link_trial/trial.sh"
backup="$here/first-link-config-backup"
alias=/etc/eso/production/connectivity.json
target=/mnt/system/etc/eso/production/connectivity.json
orig=56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470
candidate=75671e53af6dddcba7df62f26e4e1e6ec62e1097d92b836f1df9c184742249b5
bt=51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566
die(){ echo "FIRST_LINK_STOP $*";exit 2; }
hash(){
 if command -v sha256sum >/dev/null 2>&1;then sha256sum "$1"|awk '{print $1}'
 elif command -v openssl >/dev/null 2>&1;then openssl dgst -sha256 "$1"|awk '{print $NF}'
 else die HASH_TOOL_MISSING;fi
}
# Observation must preserve diagnostics even when the exact alias is absent.
# No configuration content, identifiers, keys, raw process arguments or writes.
observe_paths(){
  for file in "$target" "$alias"; do
    if [ ! -f "$file" ];then printf 'CONFIG_MISSING %s\n' "$file";continue;fi
    if [ -L "$file" ];then printf 'CONFIG_FILE_SYMLINK %s\n' "$file";fi
    resolved=$(CDPATH= cd "$(dirname "$file")" && pwd -P) || resolved=UNRESOLVED
    printf 'CONFIG_DIRECTORY %s %s\n' "$file" "$resolved"
    digest=$(hash "$file") || digest=UNREADABLE
    case "$digest" in "$orig")state=EXACT_ORIGINAL;;"$candidate")state=EXACT_CANDIDATE;;*)state=UNKNOWN_OR_UNREADABLE;;esac
    printf 'CONFIG_STATE %s %s\n' "$file" "$state"
    if [ -w "$(dirname "$file")" ];then printf 'DIRECTORY_WRITABLE_HINT %s\n' "$file";else printf 'DIRECTORY_NOT_WRITABLE_HINT %s\n' "$file";fi
  done
  if [ -f "$target" ] && [ -f "$alias" ];then
    if [ "$target" -ef "$alias" ] 2>/dev/null;then echo CONFIG_SAME_OBJECT;else echo CONFIG_OBJECT_IDENTITY_NOT_CONFIRMED;fi
  fi
}
collect(){
  echo MU1005_FIRST_LINK_OBSERVATION_BEGIN
  if [ ! -f /eso/bin/apps/btstack ];then echo STOCK_BTSTACK_MISSING
  elif [ "$(hash /eso/bin/apps/btstack)" = "$bt" ];then echo STOCK_BTSTACK_EXACT
  else echo STOCK_BTSTACK_NOT_EXACT;fi
  observe_paths
  sh "$trial" observe || echo STOCK_PROCESS_OBSERVATION_FAILED
  # Reuse compiled native observer; existing probe writes its own report.
  # Collect stock provider state even if native observer cannot run.
  sh "$here/observer/probe.sh" || {
    echo NATIVE_OBSERVER_UNAVAILABLE_OR_REPORT_EXISTS
    sh "$here/observer/observe_stock.sh" || echo STOCK_PROVIDER_OBSERVATION_FAILED
  }
  if [ -f "$here/observer/bt-live-result.txt" ];then echo NATIVE_REPORT_PRESENT;fi
  echo MU1005_FIRST_LINK_OBSERVATION_END_NO_CONFIG_CHANGE
}
preflight(){
 [ -f /eso/bin/apps/btstack ] || die MMX_REQUIRED
 [ "$(hash /eso/bin/apps/btstack)" = "$bt" ] || die WRONG_FIRMWARE
 [ -f "$target" ] && [ ! -L "$target" ] && [ -f "$alias" ] && [ ! -L "$alias" ] || die ACTIVE_CONFIG_UNAVAILABLE
 [ "$(CDPATH= cd "$(dirname "$alias")" && pwd -P)/connectivity.json" = "$target" ] || die ACTIVE_ALIAS_MISMATCH
 digest=$(hash "$target")
 case "$digest" in "$orig")echo ORIGINAL_CONFIG_EXACT;;"$candidate")echo CANDIDATE_CONFIG_EXACT;;*)die UNKNOWN_CONFIG;;esac
 if [ -w "$(dirname "$target")" ];then echo CONFIG_DIRECTORY_WRITABLE_HINT;else echo CONFIG_DIRECTORY_NOT_WRITABLE;fi
 sh "$trial" observe
}
case "${1:-observe}" in
 observe)
   [ "$#" -le 1 ] || die USAGE
   report="$here/first-link-result.txt"
   [ ! -e "$report" ] && [ ! -L "$report" ] || die REPORT_ALREADY_EXISTS
   (set -C; collect > "$report") || die REPORT_INCOMPLETE
   cat "$report"
   echo "REPORT_SAVED $report"
   ;;
 prepare)
   [ "$#" -eq 1 ] || die USAGE
   preflight
   sh "$trial" prepare "$target" "$alias" "$backup" CONFIRM_EXPERIMENT
   echo PREPARED_ONLY_NO_WRITE
   ;;
 apply)
   # Package cannot be activated until the reviewed path/recovery procedure
   # has been approved for this vehicle. This build has no activation token.
   die REVIEW_ONLY_APPLY_DISABLED
   ;;
 restore)
   # Reuse the earlier verified transaction for its own exact backup only.
   [ "$#" -eq 2 ] && [ "$2" = CONFIRM_EXPERIMENT ] || die CONFIRM_RESTORE
   [ -f "$backup/original.json" ] && [ -f "$backup/target.txt" ] || die NO_OWN_BACKUP
   sh "$trial" restore "$target" "$alias" "$backup" CONFIRM_EXPERIMENT
   ;;
 *)die USAGE;;
esac
