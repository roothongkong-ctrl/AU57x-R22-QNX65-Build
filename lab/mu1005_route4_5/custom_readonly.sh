#!/bin/sh
# Standalone read-only SD collection; reuse existing passed native helpers.
# No MIB backup/restore invocation, no writes outside the SD result file.
set -eu
umask 077
here=$(CDPATH= cd "$(dirname "$0")" && pwd -P) || exit 2
case "$here" in /net/mmx/fs/sda0/*|/fs/sda0/*) ;; *) echo SD_PATH_NOT_CONFIRMED; exit 2;; esac
for f in observe-identity config-path-probe; do
 [ -f "$here/$f" ] && [ -x "$here/$f" ] || { echo HELPER_UNAVAILABLE "$f"; exit 2; }
done
# Confirm the MMX has the original executable; do not assume RCC can run helpers.
[ -f /eso/bin/apps/btstack ] || { echo MMX_CONTEXT_NOT_CONFIRMED; exit 2; }
# Require host-side package verification before copy; optional vehicle verification.
if command -v sha256sum >/dev/null 2>&1; then (cd "$here" && sha256sum -c SHA256SUMS) || exit 2; fi
out="$here/observation.txt"
[ ! -e "$out" ] || { echo RESULT_ALREADY_EXISTS; exit 2; }
(set -C
 {
  echo MU1005_READ_ONLY_BEGIN
  "$here/observe-identity" || exit 2
  "$here/config-path-probe" || exit 2
  if [ -r "$here/observe_stock.sh" ]; then sh "$here/observe_stock.sh" || echo FD_QUERY_UNAVAILABLE; fi
  echo MU1005_READ_ONLY_END_NOT_INSTALL_AUTHORIZATION
 } > "$out") || { echo OBSERVATION_INCOMPLETE; exit 2; }
echo "Read-only result: $out"
