#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/.."
python3 -m unittest discover -s tests -p test_r93_plt_proof.py -v
# Optional private OEM artifact: do NOT publish it or run in public GitHub CI.
if [ -n "${R93_PRIVATE_BTSTACK:-}" ]; then
  python3 tools/r93_btstack_plt_proof.py --btstack "$R93_PRIVATE_BTSTACK" --out /tmp/r93_private_audit.json
fi
sh -n R93_SD_MIB_ROOT/mod/custom.sh
echo 'R93 original relocation offline proof tests OK'
