#!/bin/sh
# EXAMPLE ONLY. DO NOT RUN WHILE STOCK btstack IS ALREADY RUNNING.
# No changes to launch manager or firmware; requires an explicit lab session.
if [ "${AU57X_R92_LAB_EXPLICIT:-}" != "I_STOPPED_STOCK_BTSTACK" ]; then
  echo 'R92 refuses: original btstack must be stopped by a qualified lab owner' >&2
  exit 77
fi
if [ "$#" -lt 1 ]; then
  echo "usage: $0 /absolute/path/libau57x_bt_preload_qnx_REVIEW_ONLY.so [stock args...]" >&2
  exit 64
fi
R92_LIB="$1"
shift
case "$R92_LIB" in /*) ;; *) echo 'absolute preload path required' >&2; exit 65;; esac
[ -r "$R92_LIB" ] || { echo 'cannot read QNX .so' >&2; exit 66; }
[ -x /eso/bin/apps/btstack ] || { echo 'original btstack unavailable' >&2; exit 67; }
export AU57X_R92_PROBE=1
export LD_PRELOAD="$R92_LIB"
exec /eso/bin/apps/btstack "$@"
