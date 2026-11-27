#!/usr/bin/env python3
"""Verify the exact MU1005 seed archive; extract only known pinned entries."""
import hashlib, json, sys, zipfile
from pathlib import Path
root = Path(__file__).resolve().parent
m = json.loads((root / "seed_manifest.json").read_text())
z = root / m["seed_archive"]
if not z.is_file():
    sys.exit("OEM_SEED_MISSING: upload exact seed archive to oem_inputs/mu1005/")
out = Path(sys.argv[1]) if len(sys.argv) == 2 else None
with zipfile.ZipFile(z) as f:
    for item in m["required"]:
        name = item["path"]
        if f.namelist().count(name) != 1:
            sys.exit("OEM_SEED_BAD_ENTRY: " + name)
        if f.getinfo(name).file_size != item["bytes"]:
            sys.exit("OEM_SEED_BAD_SIZE: " + name)
        data = f.read(name)
        if hashlib.sha256(data).hexdigest() != item["sha256"]:
            sys.exit("OEM_SEED_BAD_SHA: " + name)
        if out is not None:
            target = out / name
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as dest:
                dest.write(data)
        print("OEM_SEED_PIN_OK: " + name)
print("DEPENDENCIES_NOT_INCLUDED; NO OEM RUNTIME PROOF")
