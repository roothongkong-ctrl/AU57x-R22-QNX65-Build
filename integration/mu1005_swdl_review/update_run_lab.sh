#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Adapted from MacsInSpace/mib-cluster-map v1.2.10 swdl/update_run.sh.
# Review-only dispatcher. No OEM paths, mount operations or executable update metadata.
set -eu
[ "$#" = 1 ] || exit 2
CARD=$(cd "$1" && pwd -P) || exit 2
case "$CARD" in /tmp/au57x-swdl-lab-*) ;; *) exit 2;; esac
[ -f "$CARD/LAB_ONLY" ] && [ ! -L "$CARD/LAB_ONLY" ] || exit 2
PKG=$CARD/WirelessCarPlay
[ -d "$PKG" ] && [ ! -L "$PKG" ] || exit 2
RESULT=$PKG/update_result.txt
# Upstream's result rotation and marker dispatch, isolated to our package.
if [ -e "$RESULT" ]; then
    [ -f "$RESULT" ] && [ ! -L "$RESULT" ] || exit 2
    N=1
    while [ -e "$PKG/update_result.$N.txt" ] || [ -L "$PKG/update_result.$N.txt" ]; do N=$((N+1)); done
    mv "$RESULT" "$PKG/update_result.$N.txt"
fi
[ ! -L "$RESULT" ] || exit 2
MODE=install
for f in "$PKG"/*; do
    [ -f "$f" ] && [ ! -L "$f" ] || continue
    case "${f##*/}" in
        *.sh) ;;
        [Uu][Nn][Ii][Nn][Ss][Tt][Aa][Ll][Ll]*) MODE=uninstall ;;
    esac
done
for f in "$PKG"/*; do
    [ -f "$f" ] && [ ! -L "$f" ] || continue
    case "${f##*/}" in
        *.sh) ;;
        [Cc][Oo][Ll][Ll][Ee][Cc][Tt]*) MODE=collect ;;
    esac
done
# Only our lab marker may be consumed; no real Swdlautorun.txt is generated.
if [ -e "$CARD/Swdlautorun.txt.lab" ]; then
    [ -f "$CARD/Swdlautorun.txt.lab" ] && [ ! -L "$CARD/Swdlautorun.txt.lab" ] || exit 2
    [ ! -e "$CARD/_Swdlautorun.txt.lab" ] && [ ! -L "$CARD/_Swdlautorun.txt.lab" ] || exit 2
    mv "$CARD/Swdlautorun.txt.lab" "$CARD/_Swdlautorun.txt.lab"
fi
{
    echo "NO INSTALL / LAB ONLY"
    echo "Mode: $MODE"
    case "$MODE" in
        install) echo "BLOCKED: OEM install backend unavailable" ;;
        uninstall) echo "BLOCKED: OEM restore backend unavailable" ;;
        collect) echo "LAB COLLECT OK: no vehicle access" ;;
    esac
} > "$RESULT"
[ "$MODE" = collect ] || exit 3
