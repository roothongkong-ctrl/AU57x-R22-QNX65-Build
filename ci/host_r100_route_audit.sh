#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 -m unittest discover -s tests -p test_r100_mu1005_route_audit.py -v
bash ci/host_r100_route_gate.sh
if [ -n "${R100_ORIGINAL_DIR:-}" ]; then
  mkdir -p r100_result
  python3 tools/r100_mu1005_route_audit.py --original-dir "$R100_ORIGINAL_DIR" --out r100_result/R100_ORIGINAL_MU1005_ROUTE_AUDIT.json
fi
printf '%s\n' 'R100_DUAL_ROUTE_AUDIT_OFFLINE_PASS_NO_INSTALL'
