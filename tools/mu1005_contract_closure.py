#!/usr/bin/env python3
"""Combine real OEM and SDK candidate symbol evidence; never claim runtime compatibility."""
import json, re, subprocess, sys
from pathlib import Path
from mu1005_oem_inventory import PIN, sha
from mu1005_original_symbol_providers import symbols
SDK={
"libc.so.3":"3ea952170deb54ee7916ff8a6d35d0cd056af3b27c13f92f319f069562d40e00",
"libecpp-ne.so.4":"de91fd0a76ade4163e8e70f1a870a040134f8e229a3ab6c5aa9d6bb6b63c1852",
"libm.so.2":"efae776a306b6840ed6734a67b8df927033f8aed8fd5c5ea80c1e12b895ae87c",
"libsocket.so.3":"965e36a0977935b315a697df269aaa7d6a22ec951ec3992d2097afccfb8aac74",
}
def sdk(prior,root):
    assert prior["btstack_sha256"]==PIN["btstack"]
    missing=set(prior["unmatched_strong_imports"])
    total=prior["strong_import_count"]; original=prior["matched_strong_import_count"]
    assert len(missing)+original==total
    providers={}
    for n,digest in SDK.items():
        f=root/n
        assert sha(f)==digest, n+" SDK candidate mismatch"
        h=subprocess.check_output(["readelf","-h",str(f)],text=True)
        assert "ELF32" in h and re.search(r"Machine:\s+ARM\s*$",h,re.M)
        d=subprocess.check_output(["readelf","-dW",str(f)],text=True)
        assert re.findall(r"\(SONAME\).*?\[([^\]]+)\]",d)==[n]
        ex=set()
        for b,v,i,s in symbols(f):
            if i=="UND" or b not in ("GLOBAL","WEAK") or v not in ("DEFAULT","PROTECTED"):continue
            ex.add(s.replace("@@","@"))
            if "@@" in s:ex.add(s.split("@@",1)[0])
        for s in missing&ex:providers.setdefault(s,[]).append(n)
    return {"original_strong_matched":original,"additional_sdk_candidate_matches":len(providers),"combined_candidate_matches":original+len(providers),"total_strong_imports":total,"remaining":sorted(missing-set(providers)),"sdk_candidate_providers":providers,"oem_abi_verified":False}
def contract(lane,air,dio):
    assert air["sha256"]==PIN["libairplay.so"] and dio["sha256"]==PIN["dio_manager"]
    ex={s["name"] for s in air["interface_candidates"] if s["direction"]=="export" and s["visibility"] in ("DEFAULT","PROTECTED")}
    di={s["name"] for s in dio["interface_candidates"] if s["direction"]=="import"}
    ai={s["name"] for s in air["interface_candidates"] if s["direction"]=="import"}
    if lane=="dio-server":
        src=Path("project/source/legacy_mu1005/airplay/native_exports.c").read_text()
        required={n for n in re.findall(r'SYM\(\w+,"([^"]+)"\)',src) if n.startswith(("AirPlay","gAirPlay"))}
        assert required
        result={"source_required":sorted(required),"missing":sorted(required-ex),"dio_airplay_import_export_intersection":sorted(di&ex)}
        assert not result["missing"]
    elif lane=="video":
        required={"AirPlayReceiverSessionScreen_StartSession","AirPlayReceiverSessionScreen_ProcessFrames","AirPlayReceiverSessionScreen_Setup"}
        nv={"NvSSVideoOpen","NvSSVideoStreamConfigure","NvSSVideoDecode","NvSSVideoClose"}
        result={"screen_exports":sorted(required&ex),"screen_exports_missing":sorted(required-ex),"nvidia_imports":sorted(nv&ai),"nvidia_imports_missing":sorted(nv-ai)}
        assert not result["screen_exports_missing"] and not result["nvidia_imports_missing"]
    elif lane=="mfi":
        required={"APSMFiPlatform_Initialize","APSMFiPlatform_CopyCertificate","APSMFiPlatform_CreateSignature","APSMFiPlatform_Finalize"}
        result={"original_exports":sorted(required&ex),"missing":sorted(required-ex),"authentication_executed":False}
        assert not result["missing"]
    else:raise ValueError("unknown lane")
    return result
def main():
    lane,first,second,out=sys.argv[1:]
    a=json.loads(Path(first).read_text())
    r=sdk(a,Path(second)) if lane=="sdk" else contract(lane,a,json.loads(Path(second).read_text()))
    r.update(lane=lane,runtime_verified=False,installable=False)
    Path(out).write_text(json.dumps(r,indent=2)+"\n")
    print("R105103_CONTRACT",json.dumps(r))
if __name__=="__main__":main()
