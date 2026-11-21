#!/usr/bin/env python3
"""Inspect review-only SD archives without extracting or writing to vehicle.
Not an installer or a path to enable firmware flashing. Fail closed.
"""
import argparse
import hashlib
import json
import re
import stat
import sys
import zipfile
from pathlib import PurePosixPath

MAX_BYTES=512*1024
MAX_ENTRIES=24
FIRMWARE="MHI2_CN_AU57x_K1006_2 / MU1005"

def safe_path(path):
    if not isinstance(path,str) or not path or path.startswith(("/","\\")) or "\\" in path:
        return False
    segments=path.split("/")
    return all(s not in ("",".","..") for s in segments)

def inspect_zip(path):
    errors=[]
    with zipfile.ZipFile(path) as z:
        infos=z.infolist()
        if len(infos)>MAX_ENTRIES:errors.append("too many entries")
        names=[i.filename for i in infos]
        if len(set(names))!=len(names):errors.append("duplicate archive paths")
        if not all(safe_path(n) for n in names):errors.append("unsafe archive path")
        if any(i.is_dir() or stat.S_ISLNK(i.external_attr>>16)
               for i in infos):errors.append("directory or symlink disallowed")
        if sum(i.file_size for i in infos)>MAX_BYTES:errors.append("archive too large")
        if not infos or "manifest.json" not in names:errors.append("manifest missing")
        if errors:return errors
        try:
            manifest=json.loads(z.read("manifest.json").decode("utf-8"))
        except (ValueError,UnicodeError,RuntimeError) as e:
            return ["bad manifest: "+str(e)]
        if manifest.get("firmware")!=FIRMWARE:errors.append("wrong firmware")
        if manifest.get("review_only") is not True or manifest.get("installable") is not False:
            errors.append("must be review-only/non-installable")
        entries=manifest.get("files",[])
        if not isinstance(entries,list) or not entries or len(entries)>MAX_ENTRIES-1:
            return errors+["invalid file inventory"]
        referenced=[]
        for entry in entries:
            if not isinstance(entry,dict):
                errors.append("invalid entry")
                continue
            name=entry.get("path")
            if (not safe_path(name) or not name.startswith("payload/") or
                not name.endswith((".txt",".md",".json"))):
                errors.append("unsafe or executable payload path")
                continue
            referenced.append(name)
            expected=entry.get("sha256")
            if not isinstance(expected,str) or not re.fullmatch(r"[a-f0-9]{64}",expected):
                errors.append("missing/invalid sha256 for "+name)
                continue
            if name not in names:
                errors.append("missing file "+name)
                continue
            info=z.getinfo(name)
            if info.file_size>MAX_BYTES:
                errors.append("file too large "+name)
                continue
            actual=hashlib.sha256(z.read(name)).hexdigest()
            if actual!=expected:errors.append("digest mismatch "+name)
        if len(referenced)!=len(set(referenced)):errors.append("duplicate manifest entries")
        if sorted(names)!=sorted(["manifest.json"]+referenced):
            errors.append("unlisted or omitted archive member")
    return errors

def main():
    p=argparse.ArgumentParser()
    p.add_argument("review_zip")
    args=p.parse_args()
    try:
        errors=inspect_zip(args.review_zip)
    except (OSError,zipfile.BadZipFile,RuntimeError) as e:
        errors=["unreadable archive: "+str(e)]
    print(json.dumps({"review_only":True,"installable":False,
                      "archive_integrity_pass":not errors,"errors":errors},
                     ensure_ascii=False))
    return 0 if not errors else 1

if __name__=="__main__":
    sys.exit(main())
