#!/usr/bin/env python3
"""Read-only focused original shutdown import/call boundary audit.
No hooks, original bytes, or vehicle writes."""
import sys,json,struct,re
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_first_link_matrix"))
from probe import Binary,load,prel_branch
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_IMM
root=Path(sys.argv[1]).resolve(strict=True)
out=Path(sys.argv[2])
b=Binary(load(root,"btstack"))
md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True
assert b.word(0x125968)==0xeb055604 and b.word(0x11de34)==0xeb0574d1
targets={0x27b180:"OSAPI_stop",0x27a614:"OSAPI_cleanup",0x125908:"BluetoothServices_stop",0x11e9b0:"BluetoothServices_start"}
def code_range(a,n):
    offset=a-b.va
    if offset<0 or offset+n>len(b.code):raise ValueError("not text")
    return b.code[offset:offset+n]
def edge(a,target):
    return {"site":hex(a),"target":hex(target),"labels":b.labels(target),"owner":b.owner(a)}
direct={v:[] for v in targets.values()}
for o in range(0,len(b.code)-3,4):
    a=b.va+o;word=struct.unpack_from("<I",b.code,o)[0]
    target=prel_branch(word,a)
    if target in targets:direct[targets[target]].append(edge(a,target))
# Follow only the actual stop/cleanup body into imported APIs. Record all
# direct calls plus indirect call locations; do not claim unresolved indirect
# paths have been excluded. Known function size from symbols, bounded fallback.
def inspect(a):
    sizes=[n for v,n,s in b.funcs if v==a and 0<n<=2048]
    n=min(sizes) if sizes else 0x180
    calls=[];indirect=[]
    for i in md.disasm(code_range(a,n),a):
        if i.mnemonic.startswith("bl") and i.operands:
            op=i.operands[0]
            if op.type==ARM_OP_IMM:calls.append(edge(i.address,op.imm&0xffffffff))
            else:indirect.append(hex(i.address))
    return {"entry":hex(a),"bound":n,"symbol_sized":bool(sizes),"calls":calls,"indirect_calls":indirect}
bodies={name:inspect(a) for a,name in targets.items()}
# Imported relocation-backed symbols only: candidate single interception
# boundary, not a proof that every virtual stop dispatch reaches it.
rx=re.compile(r"(ActiveObject|OSAPI|SharedPtr).*(stop|Stop|D[012]Ev|destroy)|pthread_(mutex_(trylock|lock|unlock)|join)",re.I)
imports=[]
for slot,rows in b.reloc.items():
    for r in rows:
        name=r.get("symbol") or ""
        if r.get("symbol_va")==0 and rx.search(name):
            pls=[hex(x) for x,s in b.plt.items() if s==name]
            imports.append({"slot":hex(slot),"symbol":name,"plt":pls,"type":r["type"]})
result={"scope":"ORIGINAL_STOP_IMPORT_BOUNDARY_CANDIDATES_ONLY","direct_edges":direct,"bodies":bodies,"imports":imports,"all_indirect_paths_proven":False,"live_lease_proven":False,"installable":False}
assert any(x["site"]=="0x125968" for x in direct["OSAPI_stop"])
assert any(x["site"]=="0x11de34" for x in direct["OSAPI_stop"])
out.write_text(json.dumps(result,indent=2)+"\n")
for name,body in bodies.items():print(name,json.dumps(body))
print("STOP_RELOCATION_CANDIDATES",json.dumps(imports))
print("R105198_ORIGINAL_STOP_IMPORT_AUDIT_ONLY_NO_INSTALL")
