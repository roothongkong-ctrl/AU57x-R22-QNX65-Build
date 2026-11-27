#!/usr/bin/env python3
"""Host lab only. Four strategy labels exercise one existing supervised
transaction on the SAME disposable target. No vehicle paths, no production
commands, no fabricated wireless acceptance. Each route has its own journal."""
import os, subprocess, tempfile, stat
from pathlib import Path
NAMES=("btstack.fixture","dio.fixture","si.fixture","iap2.fixture")
ROUTES=("factory-config","mib-independent","wired-receiver-reuse","native-graft")
MODES={"ok","bad-health","start-fails","timeout","stop-candidate-fails","crash-after-start","stop-original-fails"}
def files_ok(target,kind):
    for i,n in enumerate(NAMES):
        p=target/n
        s=p.lstat()
        if not stat.S_ISREG(s.st_mode) or s.st_nlink!=1 or s.st_uid!=os.getuid() or s.st_gid!=os.getgid() or stat.S_IMODE(s.st_mode)!=(0o755 if i<3 else 0o640) or p.read_bytes()!=(kind+" "+n).encode():return False
    return set(x.name for x in target.iterdir())==set(NAMES)
def phase(root):
    p=root/"state"/"journal"
    return p.read_text().split()[-1] if p.is_file() else "NONE"
def run_lab(binary,modes):
    if len(modes)!=4 or any(x not in MODES for x in modes):raise ValueError("fixture modes only")
    roots=[];events=[]
    try:
        for _ in ROUTES:
            temp=tempfile.TemporaryDirectory(prefix="mu1005-c-txn-",dir="/tmp")
            roots.append(temp)
        initial=Path(roots[0].name)/"target";initial.mkdir()
        for i,n in enumerate(NAMES):
            p=initial/n;p.write_text("original "+n);p.chmod(0o755 if i<3 else 0o640)
        identity=initial.stat().st_ino;target=initial
        for i,(route,mode) in enumerate(zip(ROUTES,modes)):
            root=Path(roots[i].name)
            if not files_ok(target,"original") or target.stat().st_ino!=identity:return "BLOCKED",events
            if i:
                # Same directory/inode, not a reset to clean synthetic files.
                target.rename(root/"target");target=root/"target"
            (root/"LAB_ONLY").touch()
            for sub in ("backup","payload","state"):(root/sub).mkdir()
            for j,n in enumerate(NAMES):
                p=root/"payload"/n;p.write_text("candidate "+n);p.chmod(0o755 if j<3 else 0o640)
            env={k:v for k,v in os.environ.items() if not k.startswith("TXN_")}
            env["TXN_LIFE_MODE"]=mode
            try:
                result=subprocess.run([str(binary),str(root),"supervised-trial"],env=env,capture_output=True,text=True,timeout=8)
            except subprocess.TimeoutExpired:
                events.append((route,"BLOCKED_TIMEOUT"));return "BLOCKED",events
            # A return code or journal alone is insufficient. Reuse actual
            # txn restore result plus independent original-file check.
            if result.returncode==0 and phase(root)=="COMMITTED" and files_ok(target,"candidate"):
                events.append((route,"FIXTURE_SELECTED"));return "SELECTED",events
            if result.returncode==3 and phase(root)=="RESTORED" and files_ok(target,"original") and "start-original" in result.stdout.splitlines():
                events.append((route,"RESTORED"));continue
            events.append((route,"BLOCKED"));return "BLOCKED",events
        return "ALL_RESTORED",events
    finally:
        # Disposable host-only directory cleanup; not real recovery.
        for tmp in reversed(roots):tmp.cleanup()
