#!/bin/sh
# R105.004 — one-shot, read-only MU1005 iAP2/Bluetooth inventory.
# Run only from M.I.B. Advanced Settings -> Run Custom Script.
# Does NOT modify config, restart services, inject code, or install anything.
# Output may contain Bluetooth MAC addresses; keep the SD folder private.
SD=/net/mmx/fs/sda0
OUT="$SD/R105_004_IAP2_PROOF"
if [ ! -d "$SD" ]; then
  echo 'R105.004: SD1 path absent'
  exit 1
fi
umask 077
mkdir -p "$OUT/configs" || { echo 'R105.004: cannot create output folder'; exit 2; }

{
  echo 'R105.004 MU1005 READ-ONLY IAP2/BLUETOOTH PROBE'
  echo 'NO CONFIG WRITE / NO RESTART / NO LD_PRELOAD / NO INSTALL'
  date 2>&1
  echo '--- identity and build ---'
  uname -a 2>&1
  echo '--- key OEM processes ---'
  pidin ar 2>&1
  for PROC in bluetooth btstack connectivity_launcher dio_manager mm-ipod; do
    echo "### process=$PROC info"
    pidin -p "$PROC" info 2>&1
    echo "### process=$PROC mapped modules"
    pidin -p "$PROC" mem 2>&1
  done
  echo '--- known executable and driver candidates ---'
  for P in \
    /eso/bin/apps/bluetooth \
    /eso/bin/apps/btstack \
    /eso/bin/apps/connectivity_launcher \
    /eso/bin/apps/dio_manager \
    /eso/lib/ipod-drvr-iap2.so \
    /eso/lib/dll/ipod-drvr-iap2.so \
    /mnt/app/eso/bin/apps/bluetooth \
    /mnt/app/eso/bin/apps/btstack \
    /mnt/app/eso/bin/apps/connectivity_launcher \
    /mnt/app/eso/bin/apps/dio_manager \
    /mnt/app/eso/lib/ipod-drvr-iap2.so \
    /mnt/app/eso/lib/dll/ipod-drvr-iap2.so \
    /etc/mm/iap2.cfg; do
    if [ -e "$P" ]; then
      echo "FOUND $P"
      ls -l "$P" 2>&1
      if [ -f "$P" ] && command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$P" 2>&1
      fi
    fi
  done
  echo '--- bounded config discovery ---'
  for ROOT in /eso /etc/mm /mnt/app/eso; do
    if [ -d "$ROOT" ]; then
      echo "### find under $ROOT (matching names only)"
      find "$ROOT" -type f \( \
        -name 'connectivity.json' -o \
        -name 'bluetooth.json' -o \
        -name 'bluetooth*.json' -o \
        -name 'iap2.cfg' -o \
        -name '*bluetooth*.cfg' \
      \) -print 2>&1
    fi
  done
  echo '--- selected config copies and hashes ---'
  for P in \
    /eso/etc/connectivity.json \
    /eso/mm/connectivity.json \
    /etc/connectivity.json \
    /mnt/app/eso/etc/connectivity.json \
    /mnt/app/eso/mm/connectivity.json \
    /mnt/app/eso/config/connectivity.json \
    /eso/etc/bluetooth.json \
    /eso/mm/bluetooth.json \
    /etc/bluetooth.json \
    /etc/mm/iap2.cfg \
    /mnt/app/etc/mm/iap2.cfg \
    /mnt/app/eso/etc/bluetooth.json \
    /mnt/app/eso/mm/bluetooth.json; do
    if [ -r "$P" ] && [ -f "$P" ]; then
      DEST="$OUT/configs/$(echo "$P" | sed 's#/#_#g')"
      echo "COPY $P -> $DEST"
      cp "$P" "$DEST" 2>&1
      if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$P" "$DEST" 2>&1
      else
        cksum "$P" "$DEST" 2>&1
      fi
      ls -l "$P" "$DEST" 2>&1
    fi
  done
  echo '--- iAP2/BT related runtime logs ---'
  for P in /var/log/messages /var/log/system /tmp/slog2info.log /tmp/au57x_r92_bt_preload.log; do
    if [ -r "$P" ]; then
      echo "LOG_PRESENT $P"
      ls -l "$P" 2>&1
      if [ "$P" = /tmp/au57x_r92_bt_preload.log ]; then
        cat "$P" 2>&1
      fi
    fi
  done
  echo '--- end of passive inventory ---'
} > "$OUT/diag.txt" 2>&1

{
  echo 'R105.004 COMPLETE'
  echo 'Read-only inventory only; no vehicle files were modified.'
  echo 'Inspect diag.txt for FOUND paths and process maps.'
  echo 'configs/ may contain Bluetooth MAC addresses; do not post publicly.'
} > "$OUT/README.txt"
echo 'R105.004 COMPLETE: SD1 /R105_004_IAP2_PROOF/'
echo 'No settings or vehicle software were changed.'
exit 0
