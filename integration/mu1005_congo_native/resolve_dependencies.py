"""Resolve transitive ARM ELF dependencies from the hash-pinned local OEM tree.
No substitute/stub libraries, no ambiguous OEM choices, no proprietary bytes in reports.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
from elf_needed import needed

BASE = {"libc.so.3", "libm.so.2", "libsocket.so.3"}

def metadata_aliases(oem, dep):
    """Resolve only original ImageFS link records, never create host links.
    Absolute paths are rooted in the same extracted image; /proc/boot is
    the QNX ImageFS mount, not a host directory. Reject traversal/cycles.
    """
    import posixpath
    from pathlib import PurePosixPath
    for meta in sorted(oem.rglob("stage2.json")):
        if meta.parent.name != "metadata":
            continue
        root = (meta.parent.parent / "filesystems" / "stage2").resolve()
        if not root.is_relative_to(oem) or not root.is_dir():
            continue
        records = {}
        for rec in json.loads(meta.read_text()).get("entries", []):
            name = rec.get("path")
            if isinstance(name, str):
                records.setdefault(name, []).append(rec)
        for name, rs in records.items():
            if PurePosixPath(name).name != dep or len(rs) != 1 or rs[0].get("type") != "symlink":
                continue
            cur = name
            chain = []
            for _ in range(16):
                if cur in chain or len(records.get(cur, [])) != 1:
                    break
                rec = records[cur][0]
                chain.append(cur)
                if rec.get("type") == "file" and rec.get("materialized") is True:
                    src = (root / cur).resolve()
                    if src.is_relative_to(root) and src.is_file():
                        digest = hashlib.sha256(src.read_bytes()).hexdigest()
                        if digest == rec.get("sha256"):
                            yield src, {"metadata": str(meta.relative_to(oem)), "chain": chain}
                    break
                if rec.get("type") != "symlink":
                    break
                target = rec.get("target", "")
                if not isinstance(target, str) or not target or "\\x00" in target:
                    break
                if target.startswith("/proc/boot/"):
                    target = target[len("/proc/boot/"):]
                elif target.startswith("/"):
                    target = target.lstrip("/")
                else:
                    target = posixpath.join(posixpath.dirname(cur), target)
                cur = posixpath.normpath(target)
                if cur in (".", "..") or cur.startswith("../") or cur.startswith("/"):
                    break

def close(stage, sdk, oem, reader=needed):
    oem = oem.resolve()
    pending = sorted(stage.iterdir())
    visited, unresolved, provenance = set(), {}, {}
    while pending:
        p = pending.pop()
        if p.name in visited:
            continue
        visited.add(p.name)
        for dep in reader(p):
            if dep in BASE:
                continue
            if not dep or Path(dep).name != dep or dep in (".", ".."):
                raise ValueError("unsafe dependency")
            dest = stage / dep
            if dest.is_file():
                pending.append(dest)
                continue
            if dep in unresolved:
                continue
            candidates = {}
            for candidate in sorted(oem.rglob(dep)):
                if not candidate.is_file() or not candidate.resolve().is_relative_to(oem):
                    continue
                try:
                    reader(candidate)
                except ValueError:
                    continue
                digest = hashlib.sha256(candidate.read_bytes()).hexdigest()
                candidates.setdefault(digest, []).append(candidate)
            aliases = []
            for candidate, chain in metadata_aliases(oem, dep):
                try:
                    reader(candidate)
                except ValueError:
                    continue
                digest = hashlib.sha256(candidate.read_bytes()).hexdigest()
                candidates.setdefault(digest, []).append(candidate)
                aliases.append(chain)
            if len(candidates) > 1:
                unresolved[dep] = {"reason":"ambiguous OEM candidates",
                                   "hashes":sorted(candidates)}
                continue
            if candidates:
                digest, paths = next(iter(candidates.items()))
                source = paths[0]
                provenance[dep] = {"origin":"pinned MU1005", "sha256":digest,
                                   "paths":[str(x.relative_to(oem)) for x in paths], "imagefs_links":aliases}
            elif (sdk / dep).is_file():
                source = sdk / dep
                reader(source)
                provenance[dep] = {"origin":"pinned SDK",
                                   "sha256":hashlib.sha256(source.read_bytes()).hexdigest()}
            else:
                unresolved[dep] = {"reason":"not present in OEM or SDK"}
                continue
            shutil.copy2(source, dest)
            pending.append(dest)
    return {"staged":sorted(visited), "unresolved":sorted(unresolved),
            "unresolved_detail":unresolved, "provenance":provenance,
            "authentication_stubs":False, "vehicle_access":False}

def self_test():
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        stage, sdk, oem = [root / n for n in ("stage","sdk","oem")]
        for d in (stage, sdk, oem, oem/"a", oem/"b"):
            d.mkdir()
        def write(p, value):
            p.write_text(value)
        def read(p):
            return p.read_text().split()
        write(stage/"helper", "lib1 libmissing libamb")
        write(oem/"a"/"lib1","lib2")
        write(oem/"b"/"lib1","lib2")
        write(oem/"a"/"lib2","lib1 libsdk")
        write(sdk/"libsdk","")
        write(oem/"a"/"libamb","x")
        write(oem/"b"/"libamb","y")
        write(sdk/"libamb","")
        outside = root/"libmissing"
        write(outside,"")
        (oem/"a"/"libmissing").symlink_to(outside)
        tree=oem/"stage2"/"filesystems"/"stage2"
        tree.mkdir(parents=True)
        meta=oem/"stage2"/"metadata"
        meta.mkdir()
        write(tree/"libreal.so","")
        write(stage/"helper","lib1 libmissing libamb libalias.so libloop.so")
        (meta/"stage2.json").write_text(json.dumps({"entries":[
            {"path":"libalias.so","type":"symlink","target":"/proc/boot/libreal.so"},
            {"path":"libreal.so","type":"file","materialized":True,"sha256":hashlib.sha256(b"").hexdigest()},
            {"path":"libloop.so","type":"symlink","target":"libloop.so"}
        ]}))
        report = close(stage,sdk,oem,read)
        assert report["staged"] == ["helper","lib1","lib2","libalias.so","libsdk"], report
        assert report["unresolved"] == ["libamb","libloop.so","libmissing"], report
        assert len(report["provenance"]["lib1"]["paths"]) == 2
        assert not (stage/"libamb").exists()
        assert report["provenance"]["libalias.so"]["imagefs_links"][0]["chain"] == ["libalias.so","libreal.so"]
    print("RECURSIVE_OEM_DEPENDENCY_CLOSURE_FIXTURE_PASS")

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--self-test", action="store_true")
    for name in ("stage","sdk","oem","report"):
        p.add_argument("--"+name, type=Path)
    a = p.parse_args()
    if a.self_test:
        self_test()
    else:
        report = close(a.stage,a.sdk,a.oem)
        a.report.write_text(json.dumps(report,indent=2)+"\n")
        print(json.dumps(report,indent=2))
        if report["unresolved"]:
            raise SystemExit("Unresolved genuine dependencies: "+", ".join(report["unresolved"]))
