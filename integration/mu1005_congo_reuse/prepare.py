#!/usr/bin/env python3
"""Prepare a wireless-only Congo port workspace, not a vehicle installer.

Use the pinned upstream wcp.tar.gz and original MU1005 recovery scripts.
Do not execute the upstream installer or transplant QC drivers/HMI wholesale.
No test runner is invoked. Native activation is deliberately not implemented.
"""
import argparse
import hashlib
import json
import tarfile
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = "223b42f90bf7c15fbbbef23259da36caa9bd4994"
CORE = {
    "libbthook.so": "bluetooth-native",
    "wcpd": "wireless-session",
    "wcppaird": "wireless-session",
    "libwcpstart.so": "receiver-native",
    "wcp-boot.sh": "startup",
    "dio-up.sh": "startup",
    "btstack": "bluetooth-native",
    "dio_manager": "receiver-native",
    "start-ap.sh": "wifi-native",
    "WCP.jar": "hmi-native",
    "librgi_hook.so": "receiver-native",
}
RECOVERY = ROOT / "integration/mu1005_vehicle_stage"

def digest(data):
    return hashlib.sha256(data).hexdigest()

def classify(path):
    parts = path.parts
    name = path.name
    if any(p in ("drv", "drivers", "pair", "logs") for p in parts):
        return None
    if name in CORE:
        return CORE[name]
    # Retain wireless daemon libraries/scripts as reference dependencies.
    if parts[:3] == ("mnt", "ota", "wcp"):
        if name.startswith(("aaw", "wcp-test", "wcp-log")):
            return None
        if name.endswith((".so", ".sh", ".conf")) or ".so." in name or "nv" in parts:
            return "wireless-dependencies"
    return None

SD = """#!/bin/sh
# Read-only preparation and recovery staging; NOT a live installer.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
case "$HERE/" in /fs/sda0/*|/fs/sdb0/*) ;; *) echo 'Use SD card'; exit 1;; esac
case "${1:-}" in
 backup)
   [ "$#" -eq 2 ] || exit 2
   sh "$HERE/backup.sh" vehicle "$2"
   sh "$HERE/recover.sh" verify "$2" "$HERE"
   ;;
 restore-stage)
   [ "$#" -eq 3 ] || exit 2
   sh "$HERE/recover.sh" stage "$2" "$3"
   ;;
 install)
   echo 'MU1005 native adapter incomplete; no live installation performed.'
   exit 1
   ;;
 *) echo 'backup SD_BACKUP_DIR | restore-stage SD_BACKUP_DIR SD_RECOVERY_PARENT'; exit 2;;
esac
"""

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("archive", type=Path)
    ap.add_argument("license", type=Path)
    ap.add_argument("out", type=Path)
    args = ap.parse_args()
    notice = args.license.read_bytes()
    if b"Required Notice:" not in notice or b"PolyForm Noncommercial" not in notice:
        raise SystemExit("Missing upstream license/notice")
    files = {}
    rows = []
    seen = set()
    with tarfile.open(args.archive, "r:gz") as src:
        for m in src:
            name = m.name
            while name.startswith("./"):
                name = name[2:]
            p = PurePosixPath(name)
            if p.is_absolute() or ".." in p.parts:
                raise SystemExit("Unsafe upstream member")
            group = classify(p)
            if not group or not m.isfile():
                continue
            key = str(p)
            if key in seen:
                raise SystemExit("Duplicate upstream member: " + key)
            seen.add(key)
            data = src.extractfile(m).read()
            dst = "upstream-reference/" + group + "/" + key
            files[dst] = data
            rows.append({"source": key, "workspace": dst, "group": group,
                         "sha256": digest(data), "mode": oct(m.mode & 0o777),
                         "status": "unmodified_reference"})
    missing = sorted(set(CORE) - {PurePosixPath(x).name for x in seen})
    port = json.loads((RECOVERY / "congo_port_table.json").read_text())
    files["mu1005/congo_port_table.json"] = (RECOVERY / "congo_port_table.json").read_bytes()
    for name in ("backup.sh", "recover.sh", "config_transaction.sh", "stock_config_apply.sh"):
        files["sd-recovery/" + name] = (RECOVERY / name).read_bytes()
    files["sd-recovery/prepare.sh"] = SD.encode()
    files["LICENSE-CONGO.txt"] = notice
    report = {
        "firmware": "MHI2_CN_AU57x_K1006_2 / MU1005",
        "upstream_commit": UPSTREAM,
        "upstream_archive_sha256": digest(args.archive.read_bytes()),
        "installable": False,
        "missing_upstream_components": missing,
        "components": rows,
        "native_mapping": port["entries"],
        "remaining_native_work": [
            "Congo libbthook build selection and MU1005 native callback ABI",
            "Congo receiver hook build selection, MU1005 dio_manager and HMI ABI",
            "Native AP/MFi paths and USB lifecycle; preserve stock authentication",
            "Reversible live installation only after native adapters are complete"
        ],
        "policy": "Stock wired CarPlay remains intact; no driver or live OEM file replacement."
    }
    files["manifest.json"] = (json.dumps(report, ensure_ascii=False, indent=2) + "\n").encode()
    files["NO-INSTALL.txt"] = (
        "Wireless-only source-material workspace. Not an installable package.\n"
        "Congo native binaries are untouched QC reference files.\n"
        "SD scripts only back up, verify and stage recovery; no live restore or activation.\n"
    ).encode()
    with zipfile.ZipFile(args.out, "x", zipfile.ZIP_DEFLATED) as out:
        for name, data in sorted(files.items()):
            out.writestr(name, data)

if __name__ == "__main__":
    main()
