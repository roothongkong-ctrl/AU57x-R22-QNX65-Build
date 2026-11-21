#!/usr/bin/env python3
"""Fully isolated SD transaction preflight and rollback simulator.
Only works in explicitly marked temporary fixtures; never mounts or writes
a car, firmware image, partition, original QNX binary or system directory.
"""
import hashlib
import os
from pathlib import Path

MARKER=".AU57X_CLOUD_SIM_ONLY"
MAX_FILE=256*1024

class ReviewFailure(Exception):
    pass

def digest(data):
    return hashlib.sha256(data).hexdigest()

def safe_name(name):
    return (isinstance(name,str) and name and not name.startswith(("/","\\"))
            and "\\" not in name and all(x not in ("",".","..")
            for x in name.split("/")) and name.startswith("fake/"))

def _scope(root):
    root=Path(root)
    if root.is_symlink() or not root.is_dir() or root.name[:10]!="au57x-sim-":
        raise ReviewFailure("not marked simulation scratch")
    if not (root/MARKER).is_file() or (root/MARKER).is_symlink():
        raise ReviewFailure("no explicit simulation marker")
    if (root/MARKER).read_text(encoding="ascii")!="SIMULATION_ONLY\n":
        raise ReviewFailure("wrong simulation marker")
    return root

def _target(root,name):
    if not safe_name(name):
        raise ReviewFailure("unsafe relative path")
    path=root.joinpath(*name.split("/"))
    if not path.is_file() or path.is_symlink():
        raise ReviewFailure("target missing, symlink or non-file")
    cursor=path.parent
    while cursor!=root:
        if cursor.is_symlink():
            raise ReviewFailure("symlink component")
        cursor=cursor.parent
    return path

def apply_review(root, changes, fail_after=None):
    """Simulates old/new content with bytes and hashes, no install script.

    On simulated failure, rollback uses preflight memory snapshot. This tests
    ordering and failure response, NOT physical power-loss/journaling safety.
    """
    root=_scope(root)
    if not isinstance(changes,list) or not changes or len(changes)>16:
        raise ReviewFailure("invalid change count")
    checked=[]
    names=set()
    # Entire preflight MUST succeed before the first mutation.
    for item in changes:
        if not isinstance(item,dict):
            raise ReviewFailure("not a manifest record")
        name=item.get("path")
        if name in names:
            raise ReviewFailure("duplicate manifest target")
        names.add(name)
        path=_target(root,name)
        old=path.read_bytes()
        new=item.get("payload")
        if not isinstance(new,bytes) or len(new)>MAX_FILE or len(old)>MAX_FILE:
            raise ReviewFailure("invalid payload length")
        if digest(old)!=item.get("old_sha256") or digest(new)!=item.get("new_sha256"):
            raise ReviewFailure("target or payload hash mismatch")
        checked.append((path,old,new))
    written=[]
    try:
        for path,old,new in checked:
            path.write_bytes(new)  # ONLY in simulation marker/temporary fixture
            written.append((path,old))
            if digest(path.read_bytes())!=digest(new):
                raise ReviewFailure("post-write integrity mismatch")
            if fail_after is not None and len(written)==fail_after:
                raise ReviewFailure("simulated failure")
    except BaseException:
        # Do not hide rollback errors; failure of any rollback is a hard error.
        failed=[]
        for path,old in reversed(written):
            try:
                path.write_bytes(old)
                if digest(path.read_bytes())!=digest(old):
                    failed.append(str(path))
            except OSError:
                failed.append(str(path))
        if failed:
            raise ReviewFailure("simulated rollback failed: "+",".join(failed))
        raise
    return {"verdict":"SIMULATED_TRANSACTION_ONLY",
            "writes":len(written),"vehicle_installable":False}
