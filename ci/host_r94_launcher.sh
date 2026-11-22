#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/.."
python3 -m unittest discover -s tests -p test_r94_launcher_patch.py -v
python3 -m unittest discover -s tests -p test_r94_launcher_audit.py -v
# Native QNX launcher is a private evidence source only, never shipped.
if [ -n "${R94_PRIVATE_LAUNCHER:-}" ]; then
  python3 tools/r94_launcher_audit.py --elf "$R94_PRIVATE_LAUNCHER" --out /tmp/r94_launcher_static.json
fi
sh -n R93_SD_MIB_ROOT/mod/custom.sh
printf 'R94 reversible launcher staging and rollback: PASS (OFFLINE ONLY)\n'
