#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
python3 -m unittest discover -s tests -p 'test_r101_rfcomm_scn_audit.py' -v
python3 -m unittest discover -s tests -p 'test_mu1005_rf_register_sites.py' -v
if [ -f /mnt/data/R99_private/btstack ];then
  python3 tools/r101_rfcomm_scn_audit.py --btstack /mnt/data/R99_private/btstack --output r101_result/R101_MU1005_SCN_AUDIT.json
else
  echo 'Original OEM private btstack not available in CI; mock negative tests ran; no OEM PASS claim'
fi
cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -Iproject/source/legacy_mu1005/bluetooth \
  tests/test_r101_scn_contract.c \
  project/source/legacy_mu1005/bluetooth/inbound.c -o /tmp/r101-scn-range
/tmp/r101-scn-range
bash ci/host_rf_arg_experiment.sh
