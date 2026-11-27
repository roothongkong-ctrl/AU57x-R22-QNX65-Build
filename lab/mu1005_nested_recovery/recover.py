#!/usr/bin/env python3
"""Recover the ICU runtime via the real R106 mount path, without exporting OEM bytes."""
import hashlib,json,re,subprocess,sys
from pathlib import Path

def sha(p):
    h=hashlib.sha256()
    with p.open("rb") as f:
        for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
    return h.hexdigest()

def inside(root,p):
    if not p.is_file():return None
    q=p.resolve(strict=True)
    if not q.is_relative_to(root):raise ValueError("outside original tree")
    return q

def arm_elf(p):
    with p.open("rb") as f:h=f.read(20)
    return len(h)==20 and h[:6]==b"\x7fELF\x01\x01" and h[18:20]==b"\x28\x00"

def elf_data(p):
    if not arm_elf(p):return None
    d=subprocess.check_output(["readelf","-dW",str(p)],text=True,timeout=30)
    syms=subprocess.check_output(["readelf","-D","-sW",str(p)],text=True,timeout=30)
    soname=re.findall(r"\(SONAME\).*?\[([^\]]+)\]",d)
    symbol=any(re.search(r"\bicudt58_dat(?:@|\s|$)",line) and re.search(r"\bGLOBAL\b",line) and not re.search(r"\bUND\b",line) for line in syms.splitlines())
    return {"soname":soname,"exports_icudt58_dat":symbol,"sha256":sha(p),"bytes":p.stat().st_size}

def listing(iso):
    raw=subprocess.check_output(["7z","l","-slt","--",str(iso)],text=True,timeout=60,errors="replace")
    entries=[]
    for block in raw.split("----------",1)[-1].split("\n\n"):
        fields=dict(line.split(" = ",1) for line in block.splitlines() if " = " in line)
        name=fields.get("Path","").replace("\\","/")
        if re.fullmatch(r"(?:[A-Za-z0-9_.+-]+/)*libicudata\.so\.58(?:\.[0-9]+)*",name) and not any(x in (".","..") for x in name.split("/")):
            try:size=int(fields.get("Size","0"))
            except ValueError:continue
            if 0<size<=128*1024*1024:entries.append((name,size))
    return sorted(set(entries))

def run(root,out,report):
    app=root/"app70/filesystems/MMX2/app/70/default/app.img"
    efs=root/"stage2/filesystems/MMX2/efs-sys/70/default/efs-system.img"
    out.mkdir(mode=0o700)
    result={"scope":"ORIGINAL_NESTED_IMAGE_RECOVERY_NOT_RUNTIME_NOT_INSTALL","files":{},"icu_candidates":[],"icu_result":"ISO_NOT_FOUND"}
    for label,path in {
      "icu_iso":app/"eso/lib/icu.iso",
      "wlan_bt_firmware":app/"var/FwImage/w8787_wlan_SDIO_bt_SDIO.bin",
      "wlan_ap_firmware":app/"var/FwImage/sd8787_uapsta.bin",
      "si_config":efs/"etc/eso/production/smartphone_integrator.json",
      "dio_config":efs/"etc/eso/production/dio_manager.json",
      "uaputl":app/"armle/sbin/uaputl",
    }.items():
        p=inside(root,path)
        result["files"][label]={"present":bool(p)}
        if p:
            result["files"][label].update(sha256=sha(p),bytes=p.stat().st_size,source=str(p.relative_to(root)))
            if label in ("si_config","dio_config"): 
                target=out/path.name
                target.write_bytes(p.read_bytes())
    iso=inside(root,app/"eso/lib/icu.iso")
    if iso:
        result["icu_result"]="NO_COMPATIBLE_ELF"
        accepted={}
        entries=listing(iso)
        result["iso_candidate_count"]=len(entries)
        for i,(name,size) in enumerate(entries):
            p=out/("icu-candidate-"+str(i))
            with p.open("xb") as f:
                proc=subprocess.run(["7z","x","-so","-spd","--",str(iso),name],stdout=f,stderr=subprocess.DEVNULL,timeout=60)
            if proc.returncode or p.stat().st_size!=size:
                result["icu_candidates"].append({"entry":name,"status":"EXTRACTION_MISMATCH"})
                p.unlink();continue
            row=elf_data(p)
            if row is None:
                result["icu_candidates"].append({"entry":name,"status":"NOT_ARM_ELF"})
            else:
                row.update(entry=name,status="ARM_ELF")
                result["icu_candidates"].append(row)
                if row["soname"]==["libicudata.so.58"] and row["exports_icudt58_dat"]:
                    accepted[row["sha256"]]=p
        if len(accepted)==1:
            p=next(iter(accepted.values()))
            (out/"libicudata.so.58").write_bytes(p.read_bytes())
            result["icu_result"]="EXACT_ISO_ELF_RECOVERED"
        elif len(accepted)>1:result["icu_result"]="AMBIGUOUS_ELF_VARIANTS"
    result["runtime_verified"]=False
    report.write_text(json.dumps(result,indent=2)+"\n")
    print("ICU_NESTED_IMAGE_RESULT",result["icu_result"])
    for k,v in result["files"].items():print("R106_PATH",k,"PRESENT" if v["present"] else "MISSING")
    print("R105128_RECOVERY_COMPLETE_NOT_RUNTIME")

if __name__=="__main__":
    run(Path(sys.argv[1]).resolve(strict=True),Path(sys.argv[2]),Path(sys.argv[3]))
