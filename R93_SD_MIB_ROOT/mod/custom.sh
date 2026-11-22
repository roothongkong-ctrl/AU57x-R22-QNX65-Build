#!/bin/sh
# R93: M.I.B. Advanced Settings -> Run Custom Script: passive diagnostic only.
# Does NOT set LD_PRELOAD, kill/restart btstack, change firmware or install anything.
SD=/net/mmx/fs/sda0
OUT="$SD/R93_BTSTACK_PROOF"
if [ ! -d "$SD" ]; then
  echo 'R93: SD1 path absent - cannot write result'
  exit 1
fi
umask 077
mkdir -p "$OUT" || { echo 'R93: SD1 not writable'; exit 2; }
{
  echo 'R93 MU1005 ONE-SHOT BTSTACK DIAG'
  echo 'READ ONLY / NO BTSTACK RESTART / NO LD_PRELOAD INJECTION'
  date 2>&1
  echo '--- btstack file ---'
  ls -l /eso/bin/apps/btstack 2>&1
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum /eso/bin/apps/btstack 2>&1
  else
    echo 'sha256sum unavailable, expected OEM sha256 is documented in R93 ZIP'
  fi
  echo '--- pidin ar ---'
  pidin ar 2>&1
  echo '--- pidin -p btstack mem (loaded libraries) ---'
  pidin -p btstack mem 2>&1
  echo '--- pidin -p btstack info (if available) ---'
  pidin -p btstack info 2>&1
  echo '--- QNX linker and stock paths ---'
  ls -l /usr/lib/ldqnx.so.2 /proc/boot/libc.so /eso/bin/apps/connectivity_launcher 2>&1
  echo '--- candidate connectivity launcher JSON ---'
  for P in /eso/etc/connectivity.json /eso/mm/connectivity.json /etc/connectivity.json /mnt/app/eso/etc/connectivity.json /mnt/app/eso/mm/connectivity.json /mnt/app/eso/config/connectivity.json; do
    if [ -r "$P" ]; then
      echo "FOUND $P"
      ls -l "$P"
      cp "$P" "$OUT/connectivity_json_copy.txt" 2>&1
    fi
  done
  echo '--- loader log present? ---'
  if [ -r /tmp/au57x_r92_bt_preload.log ]; then
    echo 'log exists (not sufficient proof without same PID and module map):'
    cat /tmp/au57x_r92_bt_preload.log
  else
    echo 'none - expected until real OEM preload has been configured'
  fi
} > "$OUT/diag.txt" 2>&1
if [ -r /tmp/au57x_r92_bt_preload.log ]; then
  cp /tmp/au57x_r92_bt_preload.log "$OUT/loader.log" 2>/dev/null
fi
if grep -q 'libau57x_bt_preload_qnx' "$OUT/diag.txt" && \
   grep -q 'stock-image-pinned' "$OUT/diag.txt"; then
  echo 'POTENTIAL_LOADED: inspect matching PID and original firmware before marking verified' > "$OUT/status.txt"
else
  echo 'NOT_YET_PROVEN: no matching OEM btstack loader log + mapped shared library' > "$OUT/status.txt"
fi
echo 'R93 COMPLETE: result on SD1 /R93_BTSTACK_PROOF/diag.txt'
echo 'R93 did not change the car. Return SD card and share the proof folder.'
exit 0
