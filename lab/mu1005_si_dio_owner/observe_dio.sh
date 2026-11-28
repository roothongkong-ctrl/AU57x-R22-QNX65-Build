#!/bin/sh
# Read-only MU1005 DIO launch witness. Parked car, stdout only.
# No raw args, environment, credentials, process memory, radio or filesystem writes.
set -u
LC_ALL=C; export LC_ALL
if ! command -v pidin >/dev/null 2>&1; then echo PIDIN_UNAVAILABLE; exit 2; fi
echo R105288_DIO_READ_ONLY_BEGIN
for name in smartphone_integrator dio_manager connectivity_launcher servicemgrmibhigh; do
    printf 'PROCESS %s\n' "$name"
    if data=$(pidin -p "$name" ar 2>/dev/null); then
        printf '%s\n' "$data" | awk -v name="$name" '
        $1 ~ /^[0-9]+$/ {
            found=0; absolute="UNKNOWN";
            for(i=2;i<=NF;i++){
                v=$i; sub(/^.*\//,"",v);
                if(v==name){found=1;if($i=="/eso/bin/apps/" name)absolute="ESO";else if($i=="/mnt/app/eso/bin/apps/" name)absolute="MNT_APP";}
            }
            if(found&&!seen[$1]++)print "PID " $1 " PATH_CLASS " absolute
        }'
    else echo AR_QUERY_UNAVAILABLE; fi
    unset data
    # QNX versions differ in supported views; report unsupported rather than
    # treating an empty result as evidence that no process is present.
    if data=$(pidin -p "$name" family 2>/dev/null); then
        printf '%s\n' "$data" | awk '
        NR==1{for(i=1;i<=NF;i++){if(tolower($i)=="pid")p=i;if(tolower($i)=="ppid")q=i}}
        NR>1 && p&&q && $p ~ /^[0-9]+$/ && $q ~ /^[0-9]+$/ {if(!seen[$p]++)print "FAMILY PID " $p " PPID " $q}
        END{if(!p||!q)print "FAMILY_FORMAT_UNRECOGNIZED"}'
    else echo FAMILY_QUERY_UNAVAILABLE; fi
    unset data
done
echo R105288_DIO_READ_ONLY_END
