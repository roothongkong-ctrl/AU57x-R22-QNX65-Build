#!/bin/sh
# Read-only combined stock-provider observation, no process launch, no hardware access.
# Only fixed path tokens are emitted; never expose argv, keys, SSID or entire fd tables.
set -u
LC_ALL=C; export LC_ALL
if ! command -v pidin >/dev/null 2>&1; then
 echo STOCK_PROVIDER_PIDIN_UNAVAILABLE
 exit 2
fi
for name in connectivity_launcher btstack bluetooth smartphone_integrator dio_manager connectionmanager mdnsd mDNSResponder; do
 if data=$(pidin -p "$name" fds 2>/dev/null); then
  printf 'PROCESS_QUERY_OK %s\n' "$name"
  printf '%s\n' "$data" | awk -v n="$name" '
    {for(i=1;i<=NF;i++){x=$i;sub(/[,:;]$/,"",x)
      if(x=="/etc/eso/production/connectivity.json" ||
         x=="/mnt/system/etc/eso/production/connectivity.json" ||
         x=="/dev/i2c8" || x=="/dev/sdio0" ||
         x=="/dev/ipod0" || x=="/dev/eso/broker" ||
         x=="/dev/socket/mdns" || x=="/var/run/mDNSResponder" ||
         x=="/tmp/mdnsd" || x=="/tmp/uaputl.cfg")
       if(!seen[x]++)print "FD_CANDIDATE",n,x
    }}'
 else printf 'PROCESS_QUERY_UNAVAILABLE %s\n' "$name"; fi
 unset data
done
for p in /mnt/app/eso/bin/apps/btstack /mnt/app/eso/bin/apps/connectivity_launcher /mnt/app/eso/bin/apps/dio_manager /mnt/app/eso/lib/libairplay.so /mnt/app/armle/sbin/uaputl /mnt/app/armle/usr/sbin/mdnsd /mnt/app/armle/sbin/mdnsd /usr/sbin/mdnsd /dev/i2c8 /dev/ipod0 /dev/sdio0 /tmp/uaputl.cfg; do
 if [ -e "$p" ]; then printf 'PATH_PRESENT %s\n' "$p"; else printf 'PATH_NOT_OBSERVED %s\n' "$p"; fi
done
echo STOCK_PROVIDER_OBSERVATION_ONLY_NO_AUTH_OR_OWNER_PROOF
