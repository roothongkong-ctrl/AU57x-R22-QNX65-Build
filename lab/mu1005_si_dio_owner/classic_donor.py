#!/usr/bin/env python3
"""Compare published classic MHI2 donor with actual MU1005, read-only.
No donor binary installation, no cross-firmware addresses or license changes."""
import hashlib,json,sys
from pathlib import Path
from elftools.elf.elffile import ELFFile
def digest(p):
    with p.open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
def elf(p):
    with p.open("rb") as f:
        e=ELFFile(f);dyn=e.get_section_by_name(".dynamic");s=e.get_section_by_name(".dynsym")
        needed=[];soname=None;imports=set();exports=set()
        if dyn:
            for t in dyn.iter_tags():
                if t.entry.d_tag=="DT_NEEDED":needed.append(t.needed)
                if t.entry.d_tag=="DT_SONAME":soname=t.soname
        if s:
            for x in s.iter_symbols():
                if not x.name or x["st_info"]["bind"] not in ("STB_GLOBAL","STB_WEAK"):continue
                if x["st_shndx"]=="SHN_UNDEF":
                    if x["st_info"]["bind"]=="STB_GLOBAL":imports.add(x.name)
                elif x["st_other"]["visibility"] in ("STV_DEFAULT","STV_PROTECTED"):exports.add(x.name)
        return dict(arch=str(e["e_machine"]),bits=e.elfclass,little=e.little_endian,needed=needed,soname=soname,imports=imports,exports=exports)
def main(root,donor,out):
    prefix=donor/"standalone/SD_ROOT/carplay/payload/wireless"
    files={"bt-hook":prefix/"common/ota/libbtwcp.so","receiver-790":prefix/"profiles/AID10.5-17PM/wcp_receiver",
           "receiver-791":prefix/"profiles/AID12.3/wcp_receiver","video":prefix/"common/ota/wcp_video",
           "connectionmanager-1102":prefix/"common/connectionmanager"}
    baselines={}
    for l in (prefix/"native-baselines.tsv").read_text().splitlines():
        a=l.split("\t");baselines[a[0]]=a[1:]
    target={}; providers={}
    for p in root.rglob("*"):
        if p.is_symlink() or not p.is_file():continue
        try:
            with p.open("rb") as f:
                if f.read(4)!=b"\x7fELF":continue
            q=elf(p)
        except (OSError,ValueError):continue
        if q["arch"]!="EM_ARM" or q["bits"]!=32 or not q["little"]:continue
        for name in {p.name,q["soname"]}-{None}:
            providers.setdefault(name,[]).append((str(p.relative_to(root)),q["exports"]))
        if p.name in ("btstack","connectionmanager","dio_manager","smartphone_integrator","libairplay.so"):
            h=digest(p)
            target.setdefault(p.name,[]).append({"path":str(p.relative_to(root)),"sha256":h,"matches_donor_baseline":[k for k,v in baselines.items() if h in v]})
    results={}
    for name,p in files.items():
        q=elf(p)
        if q["arch"]!="EM_ARM" or q["bits"]!=32 or not q["little"]:raise ValueError("unexpected donor ABI")
        available=set();missing=[];ambiguous=[]
        for dep in q["needed"]:
            entries=providers.get(dep,[])
            if not entries:missing.append(dep);continue
            if len(entries)>1:ambiguous.append(dep)
            for _,syms in entries:available.update(syms)
        unresolved=sorted(q["imports"]-available)
        results[name]={"sha256":digest(p),"needed":q["needed"],"missing_direct_needed_in_scan":missing,
                       "ambiguous_direct_needed_in_scan":ambiguous,"strong_import_count":len(q["imports"]),
                       "not_in_direct_needed_export_union":unresolved}
        print("DONOR_ABI",name,"needed",json.dumps(q["needed"]),"missing",json.dumps(missing),"unresolved_count",len(unresolved))
    if not all(x in target for x in ("btstack","connectionmanager","dio_manager","smartphone_integrator","libairplay.so")):raise ValueError("incomplete target scan")
    print("ORIGINAL_IDENTITY_COMPARE",json.dumps(target))
    out.write_text(json.dumps({"donor_commit":"aec37f5b1c87a0d961d16fe4328ac3638d0a833e","target":target,"donor_abi":results,
      "scope":"app70 and stage2/EFS static scan; export union not actual loader resolution; no runtime compatibility claim",
      "installable":False},indent=2)+"\n")
if __name__=="__main__":main(Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve(),Path(sys.argv[3]))
