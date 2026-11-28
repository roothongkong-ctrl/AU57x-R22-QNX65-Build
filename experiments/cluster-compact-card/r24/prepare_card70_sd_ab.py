#!/usr/bin/env python3
"""Prepare A/B on separate copies of a known working v1.2.14 SD package.
A is a native layout trial. B is a dormant engineering candidate, NOT an
enabled installer: do not install before target checks. No car writes.
"""
from pathlib import Path
import argparse, shutil, re, hashlib, zipfile
p=argparse.ArgumentParser()
p.add_argument("original",type=Path,help="original working SD package root")
p.add_argument("output",type=Path,help="new empty output directory, not an SD card")
p.add_argument("--review",type=Path,required=True,help="verified ARM review ZIP")
a=p.parse_args()
src=a.original.resolve();out=a.output.resolve()
assert src.is_dir() and not out.exists() and src not in out.parents
for n in ("ClusterMap/gal_dualscreen.conf","ClusterMap/enable_hook.sh","ClusterMap/carplay/carplay_cluster.sh","ClusterMap/carplay/libcarplay_cluster.so","ClusterMap/stream-player"):
    assert (src/n).is_file(),n
assert not any(f.is_symlink() for f in src.rglob("*")),"symlinks not supported"
cfg="ClusterMap/gal_dualscreen.conf"
base=(src/cfg).read_bytes().decode("ascii")
assert "\r" not in base
def settings(s,items):
    for k,v in items.items():
        s=re.sub(r"(?m)^[ \t]*"+re.escape(k)+r"[ \t]*=.*(?:\n|$)","",s)
        s=s.rstrip("\n")+"\n"+k+"="+v+"\n"
    return s
def copy_sd(name):
    dst=out/name
    shutil.copytree(src,dst)
    if (dst/"Swdlautorun.txt").exists():
        (dst/"Swdlautorun.txt").unlink()
    return dst
out.mkdir()
stock=copy_sd("00_stock_restore")
native=copy_sd("01_native_card_trial")
(native/cfg).write_text(settings(base,{"CLUSTERMAP_CARPLAY_VIEW":"full","CLUSTERMAP_CARPLAY_SCALE":"143","CLUSTERMAP_CARPLAY_FRAME_SCALE":"100"}),encoding="ascii")
(native/"README_NATIVE_TRIAL.txt").write_text("Parked only. Manual original MIB install. Native physical scale 143 is an approximation; use existing wheel map zoom to compensate. Card size, map center and scale not guaranteed. Reinstall 00_stock_restore to restore the prior configuration using original MIB procedure.\n")
# B: prepare a review-only staging tree, preserving original package.
# Do not change install payload or enable autostart. The two required source
# confirmations must come from a real target observation, not from this tool.
review=out/"02_repaint_REVIEW_ONLY"
review.mkdir()
with zipfile.ZipFile(a.review) as z:
    manifest=dict((name.strip(),h) for h,name in (line.split(maxsplit=1) for line in z.read("SHA256SUMS.txt").decode().splitlines() if line.strip()))
    for name in ("review/player/stream-player.rgd70.review","review/hook/libcarplay_cluster.rgd70.review"):
        data=z.read(name)
        assert hashlib.sha256(data).hexdigest()==manifest[name],"hash mismatch "+name
        (review/Path(name).name).write_bytes(data)
    (review/"original-review-notes.txt").write_bytes(z.read("README_先读_不可安装.txt"))
(review/"candidate-config.txt").write_text(settings(base,{"CLUSTERMAP_CARPLAY_VIEW":"map","CLUSTERMAP_CARPLAY_ETA":"no","CLUSTERMAP_CARPLAY_CARD_POSITION":"top","CLUSTERMAP_CARPLAY_FRAME_SCALE":"100"}),encoding="ascii")
# Installer seam: dio_manager environment is capped at 10 entries and original
# launcher spawns ksh with a deliberately minimal explicit env. Do not assume
# RGI export/player flags propagate. Keep proposed changes inert for review.
(review/"required-runtime-wiring.txt").write_text(
"1. Use original MIB supervisor injection and count the added CLUSTERMAP_RGD_EXPORT=1 entry against the original 10-entry cap.\n"
"2. Launcher intentionally drops inherited env. Set CLUSTERMAP_RGD_CARD70=1, CLUSTERMAP_R17_CLEAN_MAP_111=1, CLUSTERMAP_RGD_LIVE_SOURCE_VERIFIED=1 and CLUSTERMAP_RGD_CARD_RECT only at the CarPlay player invocation, after both live-source and clean-map checks are established. Do not set any confirmation automatically.\n"
"3. Direct NvSS output bypasses the I420 renderer; check actual decoder path. No automatic change to decoder mode.\n"
"4. Verify added library imports against K1006_2. Keep original SD untouched. No installer for B is produced.\n")
print("Native trial and stock recovery copy prepared. B remains a non-installable review staging tree.")
