#!/usr/bin/env python3
"""Recover actual ICU data vs shared objects and exact R106 configuration layout."""
import hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_nested_recovery"))
from recover import inside,sha,elf_data
def iso_entries(iso):
    raw=subprocess.check_output(["7z","l","-slt","--",str(iso)],text=True,errors="replace",timeout=60)
    # 7z SLT header ends at the separator line, not at an arbitrary blank line.
    rows=[]; current={}
    for line in raw.splitlines():
        if line.startswith("Path = "):
            if current:rows.append(current)
            current={"Path":line[7:]}
        elif " = " in line and current:
            k,v=line.split(" = ",1);current[k]=v
    if current:rows.append(current)
    result=[]
    for r in rows:
        name=r["Path"].replace("\\","/")
        if r.get("Folder")=="+" or not re.fullmatch(r"(?:[A-Za-z0-9_.+-]+/)*[A-Za-z0-9_.+-]+",name):continue
        if any(x in (".","..") for x in name.split("/")):continue
        try:size=int(r.get("Size","0"))
        except ValueError:continue
        if 0<size<=128*1024*1024:result.append((name,size))
    return sorted(set(result))
def data_header(p):
    with p.open("rb") as f:b=f.read(32)
    if len(b)<24 or b[2:4]!=b"\xda\x27":return None
    return {"icu_data_magic":True,"format":b[12:16].hex(),"format_version":list(b[16:20]),"data_version":list(b[20:24])}
def copy_file(root,out,p,guest,rows):
    p=inside(root,p)
    if not p:raise ValueError("missing actual source for "+guest)
    q=out/guest.lstrip("/")
    q.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(p,q)
    rows.append({"guest_path":guest,"source":str(p.relative_to(root)),"sha256":sha(p),"bytes":p.stat().st_size})
def run(root,out,report):
    app=root/"app70/filesystems/MMX2/app/70/default/app.img"
    efs=root/"stage2/filesystems/MMX2/efs-sys/70/default/efs-system.img"
    out.mkdir(mode=0o700)
    rows=[]
    for n in ("smartphone_integrator.json","dio_manager.json","connectivity.json"):
        copy_file(root,out,efs/"etc/eso/production"/n,"/mnt/system/etc/eso/production/"+n,rows)
    iap=[p for p in root.rglob("iap2.cfg") if p.is_file() and not p.is_symlink() and p.stat().st_size<1024*1024 and sha(p)=="85039d16132d827ddcb53be82c84a2d6ec6f02be6ef33258fa33e181e443715b"]
    if not iap:raise ValueError("pinned iap2 config missing")
    copy_file(root,out,sorted(iap)[0],"/mnt/system/etc/mm/iap2.cfg",rows)
    for n in ("w8787_wlan_SDIO_bt_SDIO.bin","sd8787_uapsta.bin"):
        copy_file(root,out,app/"var/FwImage"/n,"/mnt/app/var/FwImage/"+n,rows)
    copy_file(root,out,app/"armle/sbin/uaputl","/mnt/app/armle/sbin/uaputl",rows)
    iso=inside(root,app/"eso/lib/icu.iso")
    if not iso:raise ValueError("missing ICU ISO")
    entries=iso_entries(iso)
    files=[]
    for name,size in entries:
        bn=name.rsplit("/",1)[-1]
        if not (bn.lower().endswith((".dat",".res",".cnv",".icu",".spp",".nrm",".brk",".dict")) or re.search(r"\.so(?:\.|$)",bn,re.I)):continue
        files.append({"path":name,"size":size})
    # Only sample container candidates, not hundreds of locale resources.
    selected=[(n,s) for n,s in entries if re.search(r"(?:^|/)(?:icudt58[^/]*\.dat|libicudata[^/]*\.so(?:\.[0-9]+)*)$",n,re.I)]
    results=[]
    for i,(n,size) in enumerate(selected):
        q=out/("candidate-"+str(i))
        with q.open("xb") as f:
            rc=subprocess.run(["7z","x","-so","-spd","--",str(iso),n],stdout=f,stderr=subprocess.DEVNULL,timeout=60).returncode
        if rc or q.stat().st_size!=size:
            results.append({"path":n,"status":"EXTRACT_MISMATCH"});q.unlink();continue
        h=elf_data(q)
        data=data_header(q)
        results.append({"path":n,"size":size,"sha256":sha(q),"elf":h,"data":data})
    # Check alternate original basenames by SONAME, no guessed or generated ELF.
    candidates={}
    for p in root.rglob("*"):
        if not p.is_file() or "icu" not in p.name.lower() or ".so" not in p.name.lower():continue
        q=inside(root,p)
        if not q:continue
        row=elf_data(q)
        if row and row["soname"]==["libicudata.so.58"] and row["exports_icudt58_dat"]:
            row["source"]=str(q.relative_to(root));candidates[row["sha256"]]=row
    result={"scope":"ORIGINAL_PATH_STAGING_NO_INSTALL","guest_files":rows,"iso_file_count":len(entries),
        "iso_resource_count":len(files),"iso_container_candidates":results,"matching_original_icu_elf":list(candidates.values()),
        "guest_config_aliases":{"/etc/eso/production":"/mnt/system/etc/eso/production","/etc/mm/iap2.cfg":"/mnt/system/etc/mm/iap2.cfg"},
        "runtime_ready":False}
    report.write_text(json.dumps(result,indent=2)+"\n")
    print("R106_LAYOUT_STAGED",len(rows))
    print("ICU_ISO_ENTRY_COUNT",len(entries),"RESOURCE_COUNT",len(files))
    print("ICU_CONTAINER_CANDIDATES",len(results),"ORIGINAL_ELF_MATCHES",len(candidates))
    for row in results:print("ICU_CONTAINER_TYPE",row["path"],"ARM_ELF" if row.get("elf") else "ICU_DATA" if row.get("data") else "OTHER")
    print("R105129_PRIVATE_LAYOUT_COMPLETE_NOT_RUNTIME")
if __name__=="__main__":run(Path(sys.argv[1]).resolve(strict=True),Path(sys.argv[2]),Path(sys.argv[3]))
