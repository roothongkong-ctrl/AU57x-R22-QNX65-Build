#!/usr/bin/env python3
"""Host-side preparation of existing MIB Cluster Map SD settings only.
No car writes, no new binaries. Exact card-only scaling is NOT implemented.
"""
from pathlib import Path
import argparse, os, re, shutil, sys, tempfile
p=argparse.ArgumentParser()
p.add_argument("sd",type=Path,help="existing v1.2.14 SD card root")
p.add_argument("mode",choices=["scale143","map","card","full","restore"])
a=p.parse_args()
root=a.sd.resolve()
cfg=root/"ClusterMap/gal_dualscreen.conf"
backup=root/"Card70TrialBackup/gal_dualscreen.conf.original"
if not cfg.is_file() or not (root/"ClusterMap/enable_hook.sh").is_file():
    sys.exit("Invalid existing ClusterMap SD card")
if (root/"Swdlautorun.txt").exists():
    sys.exit("Disable Swdlautorun.txt first: rename to _Swdlautorun.txt")
if cfg.is_symlink() or backup.is_symlink() or backup.parent.is_symlink():
    sys.exit("Refusing symlink")
def atomic(data):
    fd,name=tempfile.mkstemp(prefix=".card70-",dir=cfg.parent)
    try:
        with os.fdopen(fd,"wb") as f:
            f.write(data);f.flush();os.fsync(f.fileno())
        os.chmod(name,cfg.stat().st_mode & 0o777)
        os.replace(name,cfg)
        assert cfg.read_bytes()==data
    finally:
        if os.path.exists(name):os.unlink(name)
if a.mode=="restore":
    if not backup.is_file():sys.exit("No original config backup")
    atomic(backup.read_bytes());print("RESTORED: original SD settings");sys.exit(0)
if not backup.exists():
    backup.parent.mkdir(exist_ok=True)
    with backup.open("xb") as f:
        f.write(cfg.read_bytes());f.flush();os.fsync(f.fileno())
base=backup.read_bytes()
text=base.decode("ascii")
if "\r" in text:sys.exit("Expected upstream LF ASCII config")
settings={"CLUSTERMAP_CARPLAY_VIEW":("full" if a.mode=="scale143" else a.mode)}
if a.mode=="scale143":
    settings["CLUSTERMAP_CARPLAY_SCALE"]="143"
    # Existing frame scaling overrides display-density scale; disable it for this trial only.
    text=re.sub(r"(?m)^([ \t]*CLUSTERMAP_CARPLAY_FRAME_SCALE[ \t]*=.*)$",r"# CARD70 trial disabled: \1",text)
for k,v in settings.items():
    text=re.sub(r"(?m)^([ \t]*"+re.escape(k)+r"[ \t]*=.*)$",r"# CARD70 trial replaced: \1",text)
    text=text.rstrip("\n")+"\n"+k+"="+v+"\n"
atomic(text.encode("ascii"))
print("Prepared:",a.mode)
print("Original SD configuration:",backup)
print("Use upstream manual software update while parked. No exact card-only scale guaranteed.")
