#!/bin/sh
# Parked vehicle only. This script and helper read configuration metadata only.
# No config contents, credentials, write, remount, restart or process injection.
set -u
LC_ALL=C;export LC_ALL
here=$(CDPATH= cd "$(dirname "$0")" && pwd -P) || exit 2
if [ -x "$here/config-path-probe" ];then "$here/config-path-probe" || exit 2
else echo CONFIG_PATH_HELPER_MISSING;exit 2;fi
for p in /etc/eso/production/connectivity.json /mnt/system/etc/eso/production/connectivity.json /mnt/efs-system/etc/eso/production/connectivity.json /mnt/efs/etc/eso/production/connectivity.json /fs/efs-system/etc/eso/production/connectivity.json;do
 [ -f "$p" ] || continue
 if command -v sha256sum >/dev/null 2>&1;then d=$(sha256sum "$p" 2>/dev/null|awk '{print $1}')
 elif command -v openssl >/dev/null 2>&1;then d=$(openssl dgst -sha256 "$p" 2>/dev/null|awk '{print $NF}')
 else d=;fi
 case "$d" in
  56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470) state=EXACT_ORIGINAL;;
  75671e53af6dddcba7df62f26e4e1e6ec62e1097d92b836f1df9c184742249b5) state=EXACT_CANDIDATE;;
  "")state=HASH_UNAVAILABLE;;
  *)state=OTHER_BYTES;;
 esac
 printf 'CONFIG_HASH %s %s\n' "$p" "$state"
done
if command -v pidin >/dev/null 2>&1;then
 for name in connectivity_launcher bluetooth btstack smartphone_integrator;do
  if data=$(pidin -p "$name" fds 2>/dev/null);then
   printf '%s\n' "$data"|awk -v n="$name" '{for(i=1;i<=NF;i++){x=$i;sub(/[,:;]$/,"",x);if(x=="/etc/eso/production/connectivity.json"||x=="/mnt/system/etc/eso/production/connectivity.json")if(!seen[x]++)print "CONFIG_FD",n,x}}'
  else printf 'CONFIG_FD_QUERY_UNAVAILABLE %s\n' "$name";fi
  unset data
 done
fi
echo READ_ONLY_NO_CONFIG_CHANGE
