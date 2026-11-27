#!/usr/bin/env python3
"""Inspect exact firmware archive and extract only unambiguous image members. No execution."""
import hashlib, json, os, pathlib, re, subprocess, sys
archive=pathlib.Path(sys.argv[1]).resolve()
out=pathlib.Path(sys.argv[2]).resolve()
out.mkdir(parents=True,exist_ok=True)
def sha(p):
    h=hashlib.sha256()
    with p.open("rb") as f:
        for b in iter(lambda:f.read(1024*1024),b""): h.update(b)
    return h.hexdigest()
with archive.open("rb") as f:
    if f.read(6)!=b"7z\xbc\xaf\x27\x1c":
        sys.exit("OEM_DOWNLOAD_NOT_7Z")
if not 800_000_000<=archive.stat().st_size<=1_200_000_000:
    sys.exit("OEM_ARCHIVE_SIZE_UNEXPECTED")
subprocess.run(["7z","t",str(archive)],check=True,stdout=subprocess.DEVNULL,timeout=600)
listing=subprocess.check_output(["7z","l","-slt",str(archive)],text=True,timeout=120)
entries=[]
for block in listing.split("\n\n"):
    d=dict(line.split(" = ",1) for line in block.splitlines() if " = " in line)
    if "Path" not in d or "Size" not in d or d.get("Folder")=="+":
        continue
    name=d["Path"].replace("\\","/")
    if name.startswith("/") or any(x in ("..","") for x in name.split("/")):
        continue
    if re.fullmatch(r"(app\.img|mifs-stage2(?:\([^)]*\))?\.img|metainfo2\.txt)",name.rsplit("/",1)[-1],re.I):
        entries.append({"member":d["Path"],"size":int(d["Size"])})
report={"firmware":"MHI2_CN_AU57x_K1006_2_MU1005","archive_bytes":archive.stat().st_size,"archive_sha256":sha(archive),"image_candidates":entries,"archive_integrity_verified":True,"oem_image_identity_verified":False,"runtime_verified":False}
# Preserve every candidate path and size; don't pick a random image if multiple exist.
for stem,pattern in [("app",r"app\.img"),("stage2",r"mifs-stage2(?:\([^)]*\))?\.img")]:
    found=[x for x in entries if re.fullmatch(pattern,x["member"].replace("\\","/").rsplit("/",1)[-1],re.I)]
    if len(found)!=1:
        print("OEM_IMAGE_NOT_UNIQUE",stem,len(found))
        continue
    item=found[0]
    dest=out/(stem+".img")
    if dest.exists(): sys.exit("OEM_OUTPUT_ALREADY_EXISTS")
    if not 0<item["size"]<=2_000_000_000: sys.exit("OEM_IMAGE_SIZE_UNEXPECTED")
    with dest.open("xb") as f:
        subprocess.run(["7z","x","-so","-spd",str(archive),"--",item["member"]],stdout=f,check=True,timeout=600)
    if dest.stat().st_size!=item["size"]: sys.exit("OEM_EXTRACT_SIZE_MISMATCH")
    item["sha256"]=sha(dest)
    print("OEM_IMAGE_EXTRACTED",stem,item["size"],item["sha256"])
(out/"inventory.json").write_text(json.dumps(report,indent=2)+"\n")
print("OEM_ARCHIVE_INTEGRITY_OK; INTERNAL IMAGE IDENTITY AND QNX FS EXTRACTION NOT VERIFIED")
