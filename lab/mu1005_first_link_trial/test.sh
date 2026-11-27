#!/bin/bash
set -euo pipefail
case_name=$1;src=$2
root=$(cd "$(dirname "$0")/../.." && pwd)
trial="$root/lab/mu1005_first_link_trial/trial.sh"
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
tmp=$(cd "$tmp" && pwd -P)
mkdir "$tmp/system" "$tmp/bin" "$tmp/other"
cp -p "$src" "$tmp/system/connectivity.json"
cp -p "$src" "$tmp/other/connectivity.json"
ln -s "$tmp/system" "$tmp/active"
target="$tmp/system/connectivity.json";alias="$tmp/active/connectivity.json";backup="$tmp/backup"
cat >"$tmp/bin/pidin" <<'EOF'
#!/bin/sh
case "$2" in
btstack|bluetooth|smartphone_integrator) printf '%s\n' "1 2 /eso/bin/apps/$2" "SECRET_KEY=fixture";;
mm-ipod) if [ "${USB_BUSY:-0}" = 1 ];then printf '%s\n' "3 4 /usr/sbin/mm-ipod";else exit 1;fi;;
*)exit 1;;
esac
EOF
chmod 700 "$tmp/bin/pidin"
export PATH="$tmp/bin:$PATH"
sh -n "$trial"
run(){ sh "$trial" "$@" "$target" "$alias" "$backup" CONFIRM_EXPERIMENT; }
case "$case_name" in
apply-restore)
 run prepare
 cmp "$src" "$target";test ! -e "$backup"
 run apply
 echo "75671e53af6dddcba7df62f26e4e1e6ec62e1097d92b836f1df9c184742249b5  $target" | sha256sum -c -
 test -f "$backup/original.json"
 run restore
 cmp "$src" "$target";;
alias-mismatch)
 alias="$tmp/other/connectivity.json"
 if run apply;then exit 1;fi
 cmp "$src" "$target";test ! -e "$backup";;
unknown-config)
 printf '\n' >>"$target"
 cp "$target" "$tmp/changed"
 if run apply;then exit 1;fi
 cmp "$tmp/changed" "$target";test ! -e "$backup";;
usb-busy)
 export USB_BUSY=1
 if run apply;then exit 1;fi
 cmp "$src" "$target";test ! -e "$backup";;
observation)
 sh "$trial" observe >"$tmp/report"
 grep -Fx 'PROCESS_PRESENT btstack' "$tmp/report"
 grep -Fx 'PROCESS_PRESENT bluetooth' "$tmp/report"
 grep -Fx 'PROCESS_PRESENT smartphone_integrator' "$tmp/report"
 grep -Fx 'OBSERVATION_ONLY_NOT_AUTHENTICATED_SESSION' "$tmp/report"
 ! grep -E 'SECRET_KEY|/eso/bin/apps/' "$tmp/report"
 cmp "$src" "$target";test ! -e "$backup";;
*)exit 2;;
esac
echo "R105146_CASE_PASS $case_name FIXTURE_ONLY_NO_VEHICLE"
