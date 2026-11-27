#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/pidin" <<'EOF'
#!/bin/sh
case "$3" in
 mem) printf '%s\n' "1 2 /eso/bin/apps/$2" "libairplay.so @12345" "SECRET_PSK=not-for-report" ;;
 fds) printf '%s\n' "/dev/ipod0" "/etc/eso/production/connectivity.json" "SECRET_TOKEN=not-for-report" "/tmp/private_user_data" ;;
 *) exit 1;;
esac
EOF
chmod 700 "$tmp/pidin"
sh -n "$root/lab/mu1005_runtime_probe/runtime_probe.sh"
PATH="$tmp:$PATH" sh "$root/lab/mu1005_runtime_probe/runtime_probe.sh" > "$tmp/report"
grep -Fx 'R105126_RUNTIME_READ_ONLY_BEGIN' "$tmp/report"
grep -Fx 'R105126_RUNTIME_READ_ONLY_END' "$tmp/report"
grep -Fx 'PROCESS_NAME_SEEN dio_manager' "$tmp/report"
grep -Fx 'MAPPED_NAME libairplay.so' "$tmp/report"
grep -Fx 'FD_ENDPOINT /dev/ipod0' "$tmp/report"
grep -Fx 'FD_CONFIG /etc/eso/production/connectivity.json' "$tmp/report"
if grep -E 'SECRET_|private_user_data|@12345' "$tmp/report"; then exit 1; fi
echo 'RUNTIME_PROBE_FILTER_FIXTURE_PASS_NO_VEHICLE'
