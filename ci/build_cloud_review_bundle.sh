#!/usr/bin/env bash
# Produce a source-only, no-install review ZIP directly in GitHub Actions.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
OUT="$ROOT/cloud12_result"
mkdir -p "$OUT"
ZIP="$OUT/AU57x_MU1005_CLOUD_SOURCE_REVIEW_ONLY.zip"
# Never archive the working tree or any compiled SDK/binary/credentials.
git archive --format=zip --output="$ZIP" HEAD \
 README.md LICENSE NOTICE_THIRD_PARTY_CN.md docs tools tests ci project/source .github/workflows
python3 - "$ZIP" <<'PY'
import re,sys,zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    names=z.namelist()
    assert any(n=="docs/CLOUD12_LEDGER.json" for n in names)
    assert any(n=="LICENSE" for n in names)
    forbidden=[n for n in names if re.search(
      r"\.(so(?:\.[0-9]+)?|elf|img|bin|7z|key|pem|p12|pfx|o|a)$",n,re.I)]
    assert not forbidden,forbidden
    assert len(names)==len(set(names))
print("AU57X_CLOUD_SOURCE_NO_INSTALL_ZIP_PASS")
PY
sha256sum "$ZIP" > "$OUT/AU57x_MU1005_CLOUD_SOURCE_REVIEW_ONLY.zip.sha256"
printf '%s\n' 'REVIEW ONLY - NO SD INSTALLATION, NO OEM BYTES, NO APPLE MFI, NO VEHICLE VERIFICATION' > "$OUT/NOT_INSTALLABLE.txt"
