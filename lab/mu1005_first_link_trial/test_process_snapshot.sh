#!/bin/sh
# Real R106 argument layout, redacted and synthetic PID numbers. No vehicle access.
set -eu
here=$(CDPATH= cd "$(dirname "$0")" && pwd -P)
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/bin"
cat > "$tmp/bin/pidin" <<'EOF'
#!/bin/sh
[ "$#" = 1 ] && [ "$1" = ar ] || exit 2
case "$MODE" in failure) exit 3;; partial) echo '2 /eso/bin/apps/btstack btstack 1'; exit 0;; esac
printf '%s\n' 'pid Arguments' '1 procnto-smp-instr' '10 /eso/bin/apps/btstack btstack 1' '11 /eso/bin/apps/bluetooth bluetooth 4' '12 ./bin/apps/smartphone_integrator -sm-name smartphone_integrator' '13 grep -Ei btstack bluetooth mm-ipod dio_manager SECRET_KEY=fixture'
if [ "$MODE" = wired ];then printf '%s\n' '14 /mnt/app/eso/bin/apps/dio_manager' '15 mm-ipod -d iap2,config=/etc/mm/iap2.cfg';fi
if [ "$MODE" = malformed ];then printf '%s\n' 'unexpected row';fi
exit 0
EOF
chmod 700 "$tmp/bin/pidin"
export PATH="$tmp/bin:$PATH"
for MODE in baseline wired;do
 export MODE
 sh "$here/trial.sh" observe > "$tmp/result"
 for name in btstack bluetooth smartphone_integrator;do grep -Fx "PROCESS_PRESENT $name" "$tmp/result";done
 for name in mm-ipod dio_manager;do
  if [ "$MODE" = wired ];then grep -Fx "PROCESS_PRESENT $name" "$tmp/result"
  else grep -Fx "PROCESS_NOT_OBSERVED $name" "$tmp/result";fi
 done
 ! grep -E 'SECRET_KEY|/eso/bin|iap2.cfg' "$tmp/result"
done
for MODE in failure partial malformed;do
 export MODE
 if sh "$here/trial.sh" observe > "$tmp/result" 2>&1;then exit 1;fi
 grep -E '^TRIAL_STOP PROCESS_QUERY_' "$tmp/result"
 ! grep -F PROCESS_NOT_OBSERVED "$tmp/result"
done
echo R106_REAL_ARGUMENT_FORMAT_FAIL_CLOSED_PASS_NO_INSTALL
