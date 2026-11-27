#!/usr/bin/env python3
"""Extract the exact 70-variant app image from the validated firmware archive."""
import pathlib, subprocess, sys
archive,dest=map(pathlib.Path,sys.argv[1:])
listing=subprocess.check_output(["7z","l","-slt",str(archive)],text=True)
matches=[]
for line in listing.splitlines():
    if not line.startswith("Path = "):continue
    member=line[7:]
    p=pathlib.PurePosixPath(member.replace("\\","/"))
    if p.is_absolute() or ".." in p.parts:continue
    if p.parts[-5:]==("MMX2","app","70","default","app.img"):
        matches.append(member)
if len(matches)!=1:sys.exit("Expected one MMX2/app/70/default/app.img, found "+str(len(matches)))
dest.parent.mkdir(parents=True,exist_ok=True)
with dest.open("xb") as out:
    subprocess.run(["7z","x","-so","-spd",str(archive),"--",matches[0]],stdout=out,check=True,timeout=900)
with dest.open("rb") as f:
    sector=f.read(512)
if sector[:3]!=bytes.fromhex("eb1090") or sector[510:512]!=bytes.fromhex("55aa"):
    sys.exit("APP70_IMAGE_FORMAT_UNCONFIRMED")
print("APP70_EXTRACTED",dest.stat().st_size)
