#!/usr/bin/env python3
"""Offline verify a copied backup against a separately trusted explicit manifest.
No capture, restore, vehicle access or production install authorization."""
import hashlib,json,os,re,stat,sys
from pathlib import Path,PurePosixPath
def verify(root,manifest):
    root=Path(root).resolve(strict=True)
    if not root.is_dir() or not isinstance(manifest,list) or not manifest:
        raise ValueError("missing root or trusted manifest")
    seen=set(); rows=[]
    for row in manifest:
        rel=row["path"]; sha=row["sha256"]; size=row["bytes"]
        if not isinstance(rel,str) or not rel or not isinstance(sha,str) or not re.fullmatch(r"[0-9a-f]{64}",sha) or type(size)!=int or size<1:
            raise ValueError("invalid manifest entry")
        pp=PurePosixPath(rel)
        if pp.is_absolute() or any(x in ("",".","..") for x in rel.split("/")) or rel in seen:
            raise ValueError("unsafe or duplicate path")
        seen.add(rel); p=root
        for name in pp.parts:
            p=p/name
            st=p.lstat()
            if stat.S_ISLNK(st.st_mode):raise ValueError("symlink in backup")
        if not stat.S_ISREG(st.st_mode) or st.st_nlink!=1 or st.st_size!=size:
            raise ValueError("not an exact independent regular file")
        with p.open("rb") as f:
            before=os.fstat(f.fileno()); d=hashlib.file_digest(f,"sha256").hexdigest(); after=os.fstat(f.fileno())
        if (before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns)!=(st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns,st.st_ctime_ns) or (before.st_dev,before.st_ino,before.st_size)!=(after.st_dev,after.st_ino,after.st_size) or before.st_mtime_ns!=after.st_mtime_ns or before.st_ctime_ns!=after.st_ctime_ns or d!=sha:
            raise ValueError("backup identity or digest mismatch")
        rows.append({"path":rel,"bytes":size,"sha256":d})
    return {"files":rows,"backup_bytes_verified":True,"restore_verified":False,"installable":False}
if __name__=="__main__":
    if len(sys.argv)!=3:raise SystemExit("usage: backup_verify.py COPIED_BACKUP_DIR TRUSTED_MANIFEST_JSON")
    print(json.dumps(verify(sys.argv[1],json.loads(Path(sys.argv[2]).read_text())),indent=2))
