#!/bin/sh
# R105.126 MU1005 original runtime evidence, stdout only. No writes to car.
# No credentials, raw process memory, network payload, config contents or logs.
set -u
LC_ALL=C; export LC_ALL
orig=56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470
candidate=75671e53af6dddcba7df62f26e4e1e6ec62e1097d92b836f1df9c184742249b5
digest(){
 if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" 2>/dev/null | awk '{print $1}'
 elif command -v openssl >/dev/null 2>&1; then openssl dgst -sha256 "$1" 2>/dev/null | awk '{print $NF}'
 else return 1; fi
}
config(){
 [ -f "$1" ] || return 0
 d=$(digest "$1")
 case "$d" in "$orig") state=EXACT_ORIGINAL;; "$candidate") state=EXACT_CANDIDATE;; *) state=UNKNOWN;; esac
 printf 'CONFIG path=%s state=%s\n' "$1" "$state"
 # File existence, not active reader or permission proof.
}
pin(){
 [ -f "$1" ] || return 0
 d=$(digest "$1")
 case "$d" in "$2") state=MATCH;; *) state=UNKNOWN;; esac
 printf 'IMAGE path=%s state=%s\n' "$1" "$state"
}
# Allowlisted basenames, not raw pidin output (no addresses, args or user data).
markers(){
 awk '
 BEGIN {
 n=split("libiap2client.so.1 libstrm.so.1 libdns_sd.so.1 libairplay.so libnvaudio.so libnvos.so libnvrm.so libecpp-ne.so.4 ipod-drvr-iap2.so iap2_driver",a," ");
 for(i=1;i<=n;i++) wanted[a[i]]=1;
 }
 {for(i=1;i<=NF;i++){
  v=$i; sub(/^.*\//,"",v); sub(/[,:;]$/,"",v);
  if(v in wanted && !seen[v]++) print "MAPPED_NAME " v;
 }}'
}
endpoints(){
 awk '{for(i=1;i<=NF;i++){
 v=$i;sub(/[,:;]$/,"",v);
 if(v ~ /^\/dev\/(ipod[0-9]*|iap2[0-9]*|mm-ipod[0-9]*)(\/[A-Za-z0-9_.-]+)*$/ &&
    length(v)<=128 && !seen[v]++) print "FD_ENDPOINT " v;
 if(v ~ /^\/etc\/(eso\/production\/connectivity\.json|mm\/iap2\.cfg)$/ && !seen[v]++)
    print "FD_CONFIG " v;
 }}'
}
echo "R105126_RUNTIME_READ_ONLY_BEGIN"
echo "SCOPE paths_and_process_witnesses_only_not_connection"
for base in "" /mnt/app /mnt/efs-system /mnt/efs /fs/efs-system; do
 config "$base/etc/eso/production/connectivity.json"
 config "$base/etc/eso/connectivity.json"
done
for base in "" /mnt/app; do
 pin "$base/eso/bin/apps/bluetooth" 7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272
 pin "$base/eso/bin/apps/dio_manager" e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c
done
for file in /etc/mm/iap2.cfg /mnt/app/etc/mm/iap2.cfg; do
 if [ -f "$file" ]; then printf 'IAP2_CFG_PRESENT %s\n' "$file"; fi
done
for node in /dev/ipod /dev/ipod0 /dev/ipod1 /dev/iap2 /dev/iap20; do
 if [ -e "$node" ]; then printf 'NODE_PRESENT %s\n' "$node"; fi
done
if command -v pidin >/dev/null 2>&1; then
 for name in bluetooth btstack mm-ipod smartphone_integrator dio_manager mDNSResponder io-pkt-v4; do
  printf 'PROCESS_QUERY %s\n' "$name"
  # A successful pidin invocation is not proof the requested process exists.
  if data=$(pidin -p "$name" mem 2>/dev/null); then
   printf '%s\n' "$data" | awk -v name="$name" '{for(i=1;i<=NF;i++){v=$i;sub(/^.*\//,"",v);if(v==name&&!seen++){print "PROCESS_NAME_SEEN " name}}}'
   printf '%s\n' "$data" | markers
  else echo "MEM_QUERY_UNAVAILABLE"; fi
  unset data
  if data=$(pidin -p "$name" fds 2>/dev/null); then
   printf '%s\n' "$data" | endpoints
  else echo "FD_QUERY_UNAVAILABLE"; fi
  unset data
 done
else echo "PIDIN_UNAVAILABLE"; fi
echo "R105126_RUNTIME_READ_ONLY_END"
