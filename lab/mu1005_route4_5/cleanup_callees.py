#!/usr/bin/env python3
"""Follow the three *actual* original cleanup callees and enumerate direct
and file-backed pointer references to the cleanup entry. Read only.
The bounded CFG and indirect call list are evidence, not proof of exclusion."""
import json,struct,sys
from pathlib import Path
from lifecycle_cfg import walk,md
from probe import Binary,load
from capstone.arm import ARM_INS_B,ARM_INS_BL,ARM_INS_BLX,ARM_OP_IMM,ARM_CC_AL,ARM_CC_INVALID

def refs(b,targets):
    hits=[]
    for o in range(0,len(b.code)-3,4):
        a=b.va+o;word=struct.unpack_from("<I",b.code,o)[0]
        # ARM B/BL including conditional B and tail branches, no Thumb claims.
        if word&0x0e000000!=0x0a000000 or word>>28==15:continue
        d=(word&0xffffff)<<2
        if d&0x2000000:d-=0x4000000
        v=(a+8+d)&0xffffffff
        if v in targets:
            hits.append(dict(site=hex(a),target=hex(v),kind="BL" if word&0x01000000 else "B",condition=word>>28,owner=b.owner(a)))
    return hits
def pointers(b,targets):
    hits=[]
    for s in b.e.iter_sections():
        if not s["sh_flags"]&2 or s["sh_flags"]&4 or s["sh_type"]=="SHT_NOBITS":continue
        d=s.data();v=int(s["sh_addr"])
        for o in range(0,len(d)-3,4):
            a=v+o;word=struct.unpack_from("<I",d,o)[0]
            # Include relocation-adjusted values as candidates; never call them
            # live vtables unless the object and dispatch have been proved.
            rel=b.reloc.get(a,[])
            values={word}|{(r["symbol_va"]+(r["addend"] if r["addend"] is not None else word))&0xffffffff for r in rel}
            for t in sorted(values&targets):hits.append(dict(site=hex(a),target=hex(t),section=s.name,relocations=rel))
    return hits
def main(root,out):
    b=Binary(load(root,"btstack"))
    assert b.word(0x125968)==0xeb055604 and b.word(0x11de34)==0xeb0574d1
    targets={0x279f80,0x27a604,0x1e4fa8,0x27a614,0x27b180}
    ranges={"singleton":(0x279f80,0x279f80,0x27a080),
            "cleanup_first":(0x27a604,0x27a604,0x27a614),
            "cleanup_second":(0x1e4fa8,0x1e4fa8,0x1e53a8)}
    bodies={k:walk(b.word,*v,b.labels) for k,v in ranges.items()}
    edges=refs(b,targets);ptrs=pointers(b,targets)
    assert any(x["site"]=="0x27b18c" and x["target"]=="0x27a614" and x["kind"]=="B" for x in edges)
    assert any(x["site"]=="0x27a654" and x["target"]=="0x27a604" for x in edges)
    assert any(x["site"]=="0x27a658" and x["target"]=="0x1e4fa8" for x in edges)
    for k,v in bodies.items():
        indirect=[x for x in v["calls"] if x["target"] is None]
        print("CLEANUP_CALLEE",k,json.dumps({x:v[x] for x in ("entry","calls","returns","unresolved")}))
        print("CLEANUP_INDIRECT_CALLS",k,json.dumps(indirect))
        for ins in v["instructions"]:print("CLEANUP_INSTRUCTION",k,ins["va"],ins["mnemonic"],ins["op_str"])
    print("CLEANUP_DIRECT_EDGES",json.dumps(edges))
    print("CLEANUP_POINTER_CANDIDATES",json.dumps(ptrs))
    out.write_text(json.dumps(dict(scope="DIRECT_ARM_AND_BOUNDED_CALLEES_ONLY",bodies=bodies,direct_edges=edges,pointer_candidates=ptrs,live_lease_proven=False,all_indirect_paths_proven=False,installable=False),indent=2)+"\n")
    print("R105207_CLEANUP_CALLEES_READONLY_COMPLETE")
if __name__=="__main__":main(Path(sys.argv[1]).resolve(strict=True),Path(sys.argv[2]))
