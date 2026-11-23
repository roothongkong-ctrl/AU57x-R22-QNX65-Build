#!/usr/bin/env bash
# Repeatedly exercises actual native supervised install, restore and crash recovery on host.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
python3 -m unittest discover -s tests -p test_r96_oneclick_codegen.py -v
