#!/usr/bin/env python3
"""Consume exact R105.144 anchors; inspect next edges, not the same marker scans."""
import hashlib,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_first_link_matrix"))
from probe import Binary,load,prel_branch,PINS
from trace import trace
from callchain import decode
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_MEM,ARM_OP_REG,ARM_REG_PC
LANES={
"bt-start-exec":("btstack","code",[0x13e938,0x13e6b8,0x13f37c]),
"bt-sdp-exec":("btstack","code",[0x137b00,0x137898,0x137760]),
"bt-base-exec":("btstack","code",[0x132314,0x1322c4,0x132290]),
"bt-config-callee":("bluetooth","code",[0x15619c]),
"bt-config-caller":("bluetooth","code",[0x1562f8,0x15637c,0x156384]),
"driver-component-table":("ipod-drvr-iap2.so","table",[0x28318,0x284a8]),
"driver-id-table":("ipod-drvr-iap2.so","table",[0x288c8,0x288d8]),
"driver-support-table":("ipod-drvr-iap2.so","table",[0xa8df0,0xa8e14,0xa8e44,0xa8e80,0xa90a8]),
"driver-link-table":("ipod-drvr-iap2.so","table",[0xa94a4]),
"dio-connect-context":("dio_manager","code",[0x17a094])}
def owners(b,targets):
    hits=[]
    for off in range(0,len(b.code)-3,4):
        a=b.va+off;v=prel_branch(struct.unpack_from("<I",b.code,off)[0],a)
        if v in targets:hits.append({"va":hex(a),"target":hex(v),"owner":b.owner(a)})
    return hits[:120]
def code_window(b,a):
    if not b.va<=a<b.va+len(b.code):raise ValueError("anchor outside text")
    lo=max(b.va,a-32);hi=min(b.va+len(b.code),a+160)
    names={v:b.labels(v) for v in set(b.names)|set(b.plt)}
    code=b.code[lo-b.va:hi-b.va]
    calls,branches=decode(code,lo,names)
    md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True;md.skipdata=True
    ind=[];literal=[];fields=[]
    for i in md.disasm(code,lo):
        if not i.id:continue
        if i.mnemonic.startswith(("blx","bx")) and i.operands and i.operands[0].type==ARM_OP_REG:
            ind.append({"va":hex(i.address),"reg":i.reg_name(i.operands[0].reg),"kind":i.mnemonic})
        if i.mnemonic.startswith(("ldr","str")):
            for o in i.operands:
                if o.type!=ARM_OP_MEM:continue
                if o.mem.base==ARM_REG_PC and o.mem.index==0:
                    p=(i.address+8+o.mem.disp)&0xffffffff
                    v=b.word(p)
                    if v is not None:
                        literal.append({"va":hex(i.address),"slot":hex(p),"points_to_code":b.va<=v<b.va+len(b.code),"target_labels":b.labels(v)})
                elif o.mem.index==0 and abs(o.mem.disp)<=4096:
                    fields.append({"va":hex(i.address),"base":i.reg_name(o.mem.base),"offset":o.mem.disp,"kind":i.mnemonic})
    return {"anchor":hex(a),"scope":"bounded_window_not_function","calls":calls,"branches":branches,"indirect_branches":ind,"literal_slots":literal,"field_accesses":fields}
def table_edges(b,anchors):
    # Examine record neighbors and relocations, then cross-reference only plausible
    # code pointers or table entry bases. Do not claim a metadata record is a vtable.
    rows=[];targets={};seen=set()
    for a in anchors:
        if b.word(a) is None:raise ValueError("anchor not file backed")
        for v in range(a-16,a+20,4):
            if v in seen:continue
            seen.add(v);w=b.word(v)
            if w is None:continue
            rel=b.reloc.get(v,[])
            opts={w}|{(r["symbol_va"]+(r["addend"] if r["addend"] is not None else w))&0xffffffff for r in rel}
            candidates=[{"va":hex(x),"labels":b.labels(x)} for x in sorted(opts) if b.va<=x<b.va+len(b.code)]
            rows.append({"slot":hex(v),"relocations":rel,"code_candidates":candidates,"anchor":v in anchors})
            if rel:targets[v]="relocated_record_slot"
    refs=trace(b.code,b.va,b.word,targets) if targets else []
    # Inverse pointer references to a record slot are a new edge beyond R105.144.
    backlinks=[]
    for v,rels in b.reloc.items():
        w=b.word(v)
        if w is None or v in seen:continue
        opts={w}|{(r["symbol_va"]+(r["addend"] if r["addend"] is not None else w))&0xffffffff for r in rels}
        for hit in opts&set(targets):
            backlinks.append({"va":hex(v),"target":hex(hit),"relocations":rels})
    return {"anchors":[hex(a) for a in anchors],"record_neighbors":rows,"code_reference_candidates":refs[:100],"record_backlinks":backlinks[:100],"code_reference_count":len(refs),"backlink_count":len(backlinks)}
def selftest():
    assert len(LANES)==10
    assert prel_branch(0xeb000002,0x1000)==0x1010
    assert prel_branch(0xeafffffe,0x1000) is None
    print("R105145_PATH_SELFTEST_OK")
def main(lane,root,out):
    name,kind,anchors=LANES[lane];b=Binary(load(root,name))
    if kind=="code":
        result={"windows":[code_window(b,a) for a in anchors],"direct_callers":owners(b,set(anchors))}
        found=any(w["calls"] or w["indirect_branches"] for w in result["windows"])
    else:
        result=table_edges(b,anchors)
        found=bool(result["code_reference_count"] or result["backlink_count"] or any(r["code_candidates"] for r in result["record_neighbors"]))
    d={"lane":lane,"scope":"STATIC_NEXT_EDGE_CANDIDATES","sha256":PINS[name],"candidate_found":found,"result":result,"runtime_verified":False,"wireless_verified":False,"installable":False}
    out.write_text(json.dumps(d,indent=2)+"\n");print(json.dumps(d,sort_keys=True))
    if not found:raise SystemExit(3)
if __name__=="__main__":
    if sys.argv[1:]==["--selftest"]:selftest()
    else:main(sys.argv[1],Path(sys.argv[2]).resolve(strict=True),Path(sys.argv[3]))
