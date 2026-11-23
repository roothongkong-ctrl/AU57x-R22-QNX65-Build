#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
python3 -m unittest discover -s tests -p test_r96_candidate_bundle.py -v
mkdir -p r96_result
ARCHIVE=r96_result/AU57x_R96_SD_REVIEW_NOT_INSTALL.zip
if test -e "$ARCHIVE"; then
  # Rebuild to a separate temporary path and compare exact bytes. Never silently
  # accept a stale/replaced existing archive after a previous CI step.
  TEST_TMP="$(mktemp -d)"
  trap 'rm -rf "$TEST_TMP"' EXIT
  python3 tools/r96_candidate_bundle.py \
     --r95-evidence r95_result/R95_STEPS_03_TO_17_EVIDENCE.json \
     --docs docs --out "$TEST_TMP/candidate.zip"
  cmp "$ARCHIVE" "$TEST_TMP/candidate.zip"
else
  python3 tools/r96_candidate_bundle.py \
     --r95-evidence r95_result/R95_STEPS_03_TO_17_EVIDENCE.json \
     --docs docs --out "$ARCHIVE"
fi
python3 tools/sd_package_audit.py "$ARCHIVE"
