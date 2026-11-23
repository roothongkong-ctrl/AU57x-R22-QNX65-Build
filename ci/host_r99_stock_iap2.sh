#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
python3 -m unittest discover -s tests -p 'test_r99*.py' -v
python3 -m py_compile tools/r99_stock_iap2_route.py tools/r99_verify_stage.py
if [[ -n "${R99_OEM_BLUETOOTH:-}" && -n "${R99_OEM_IAP2_DRIVER:-}" ]]; then
  R99_OEM_BLUETOOTH="$R99_OEM_BLUETOOTH" R99_OEM_IAP2_DRIVER="$R99_OEM_IAP2_DRIVER" python3 - <<'PY'
import os, sys
sys.path.insert(0,'tools')
from pathlib import Path
from r99_stock_iap2_route import audit_binary, BLUETOOTH_SHA, IAP2_DRIVER_SHA, OEM_STRING_MARKERS
print('OEM_BT:',audit_binary(Path(os.environ['R99_OEM_BLUETOOTH']),BLUETOOTH_SHA,OEM_STRING_MARKERS['bluetooth']))
print('OEM_IAP2:',audit_binary(Path(os.environ['R99_OEM_IAP2_DRIVER']),IAP2_DRIVER_SHA,OEM_STRING_MARKERS['iap2_driver']))
PY
fi
printf 'R99_NATIVE_IAP2_DUAL_CONFIG_OFFLINE_TESTS_PASS__NOT_INSTALLABLE\n'
