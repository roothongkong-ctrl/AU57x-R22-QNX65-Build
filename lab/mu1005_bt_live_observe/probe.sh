#!/bin/sh
# Parked car only. Read-only; no process restart, configuration write or radio command.
set -eu
umask 077
here=$(CDPATH= cd "$(dirname "$0")" && pwd -P)
case "$here" in /fs/sda0/*|/net/mmx/fs/sda0/*) ;; *) echo SD_PATH_NOT_CONFIRMED; exit 2;; esac
[ -f /eso/bin/apps/btstack ] || { echo MMX_REQUIRED; exit 2; }
[ -x "$here/bt-live-observe" ] || { echo HELPER_MISSING; exit 2; }
if command -v sha256sum >/dev/null 2>&1; then (cd "$here" && sha256sum -c SHA256SUMS) || exit 2; fi
out="$here/bt-live-result.txt"
[ ! -e "$out" ] || { echo RESULT_ALREADY_EXISTS; exit 2; }
# pidin output is not logged. Native helper independently verifies process image.
pids=$(pidin -p btstack ar 2>/dev/null | awk '$1 ~ /^[0-9]+$/ {if(!seen[$1]++)print $1}')
set -- $pids
(set -C
 {
  echo MU1005_ONE_SHOT_BEGIN
  if [ "$#" -eq 1 ]; then
   "$here/bt-live-observe" "$1" || echo BTSTACK_SNAPSHOT_UNAVAILABLE
  else echo BTSTACK_PID_NOT_UNIQUE; fi
  sh "$here/observe_stock.sh" || echo STOCK_PROVIDER_OBSERVATION_UNAVAILABLE
  echo MU1005_ONE_SHOT_END_NOT_INSTALL_AUTHORIZATION
 } > "$out") || { echo OBSERVATION_FAILED_SEE_REPORT; exit 2; }
echo "Read-only result: $out"
