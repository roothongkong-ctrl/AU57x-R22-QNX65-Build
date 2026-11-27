#!/usr/bin/env python3
"""Pinned OEM per-component interface analysis, static evidence only."""
import argparse,json,re
from pathlib import Path
from mu1005_oem_inventory import PIN,inventory,sha
from mu1005_original_symbol_providers import symbols,checked
from mu1005_first_link_combined import scan
from r99_stock_iap2_route import BLUETOOTH_SHA,IAP2_DRIVER_SHA
PAT=re.compile(r"iap2|mfi|authenticat|airplay|carplay|bluetooth|wlan|wifi|video|display|session|receiver|transport",re.I)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("root",type=Path);p.add_argument("inventory",type=Path);p.add_argument("component");p.add_argument("out",type=Path)
    a=p.parse_args();root=a.root.resolve(strict=True);prior=json.loads(a.inventory.read_text())
    name=a.component
    if name in PIN:
        digest=PIN[name]
        matches=[checked(root,x["relative_path"],digest) for x in prior["files"][name] if x.get("matches_pinned")]
    elif name in ("bluetooth","iap2_driver"):
        digest=BLUETOOTH_SHA if name=="bluetooth" else IAP2_DRIVER_SHA
        pins,_=scan(root);matches=pins[digest]
    else:raise ValueError("unknown lane")
    if not matches:raise ValueError("exact OEM component not found")
    matches=sorted(set(matches)); target=matches[0]
    if sha(target)!=digest:raise ValueError("pin mismatch")
    inv=inventory(target);sym=symbols(target)
    strong={n for b,v,i,n in sym if b=="GLOBAL" and i=="UND"}
    providers={};unlocated=[];errors=[]
    for dep in inv["needed"]:
        entries=prior["dependency_candidates"].get(dep,[])
        if not entries:unlocated.append(dep);continue
        for x in entries:
            try:
                f=checked(root,x["resolved_path"],x["sha256"])
                exports=set()
                for b,v,i,n in symbols(f):
                    if i=="UND" or b not in ("GLOBAL","WEAK") or v not in ("DEFAULT","PROTECTED"):continue
                    exports.add(n.replace("@@","@"))
                    if "@@" in n:exports.add(n.split("@@",1)[0])
                for n in strong & exports:providers.setdefault(n,[]).append(x["resolved_path"])
            except (OSError,ValueError) as e:errors.append({"dependency":dep,"error":str(e)})
    relevant=[{"name":n,"direction":"import" if i=="UND" else "export","binding":b,"visibility":v} for b,v,i,n in sym if PAT.search(n) and b in ("GLOBAL","WEAK")]
    report={"component":name,"sha256":digest,"paths":[str(x.relative_to(root)) for x in matches],"direct_needed":inv["needed"],"strong_imports":len(strong),"matched_in_prior_original_inventory":len(providers),"unlocated_in_prior_inventory":unlocated,"provider_errors":errors,"interface_candidates":relevant,"candidate_only":True,"runtime_verified":False,"installable":False}
    with a.out.open("x") as f:json.dump(report,f,indent=2)
    print("OEM_INTERFACE_STATIC",name,len(strong),len(providers),len(relevant))
    print("DEPENDENCIES_NOT_IN_PRIOR_INVENTORY",unlocated)
if __name__=="__main__":main()
