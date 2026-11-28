#!/usr/bin/env python3
"""Exact MU1005 lifecycle control-flow audit. Read-only original ELF.
Do not conflate adjacent stripped functions; unresolved indirect branches
remain visible. No generated firmware, no install."""
from pathlib import Path
import json,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_first_link_matrix"))
from probe import Binary,load
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_IMM,ARM_OP_REG,ARM_REG_PC,ARM_REG_LR,ARM_CC_AL,ARM_CC_INVALID,ARM_INS_B,ARM_INS_BL,ARM_INS_BLX,ARM_INS_BX
from capstone import CS_GRP_JUMP,CS_GRP_CALL

md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True
def walk(read,entry,lo,hi,labels):
    pending=[entry];seen=set();edges=[];calls=[];returns=[];unknown=[];instructions=[]
    while pending:
        a=pending.pop()
        if a in seen:continue
        if a<lo or a>=hi or a%4:
            unknown.append({"site":hex(a),"kind":"outside_explicit_bound"});continue
        if len(seen)>=2048:raise ValueError("CFG budget")
        seen.add(a)
        word=read(a)
        if word is None:unknown.append({"site":hex(a),"kind":"unmapped"});continue
        ins=next(md.disasm(word.to_bytes(4,"little"),a,count=1),None)
        if ins is None:unknown.append({"site":hex(a),"kind":"undecoded"});continue
        instructions.append({"va":hex(a),"mnemonic":ins.mnemonic,"op_str":ins.op_str})
        conditional=ins.cc not in (ARM_CC_AL,ARM_CC_INVALID)
        op=ins.operands[0] if ins.operands else None
        def add(v,kind):
            edges.append({"from":hex(a),"to":hex(v),"kind":kind});pending.append(v)
        if ins.id in (ARM_INS_BL,ARM_INS_BLX) or ins.group(CS_GRP_CALL):
            calls.append({"site":hex(a),"target":hex(op.imm&0xffffffff) if op and op.type==ARM_OP_IMM else None,"labels":labels(op.imm&0xffffffff) if op and op.type==ARM_OP_IMM else [],"conditional":conditional})
            add(a+4,"call_return");continue
        if ins.id==ARM_INS_B and op and op.type==ARM_OP_IMM:
            v=op.imm&0xffffffff
            if lo<=v<hi:add(v,"branch")
            else:unknown.append({"site":hex(a),"kind":"external_branch_or_tailcall","target":hex(v),"labels":labels(v)})
            if conditional:add(a+4,"conditional_fallthrough")
            continue
        if ins.id==ARM_INS_BX:
            if op and op.type==ARM_OP_REG and op.reg==ARM_REG_LR:returns.append(hex(a))
            else:unknown.append({"site":hex(a),"kind":"indirect_branch"})
            if conditional:add(a+4,"conditional_fallthrough")
            continue
        try: writes_pc=ARM_REG_PC in ins.regs_access()[1]
        except Exception:writes_pc=False
        if writes_pc or ins.group(CS_GRP_JUMP):
            if ("pop" in ins.mnemonic or "ldm" in ins.mnemonic) and "pc" in ins.op_str and "sp" in ins.op_str or ins.mnemonic.startswith("pop") and "pc" in ins.op_str:
                returns.append(hex(a))
            else:unknown.append({"site":hex(a),"kind":"unresolved_pc_write","mnemonic":ins.mnemonic,"op_str":ins.op_str})
            if conditional:add(a+4,"conditional_fallthrough")
            continue
        add(a+4,"fallthrough")
    return {"entry":hex(entry),"range":[hex(lo),hex(hi)],"instructions":sorted(instructions,key=lambda x:int(x["va"],16)),"calls":calls,"returns":returns,"unresolved":unknown,"edges":edges}
def selftest():
    # push, bl external, pop pc, adjacent unrelated bl.
    data={0x1000:0xe92d4010,0x1004:0xeb00003d,0x1008:0xe8bd8010,0x100c:0xeb000001}
    r=walk(data.get,0x1000,0x1000,0x1010,lambda a:[])
    assert [x["site"] for x in r["calls"]]==["0x1004"]
    assert r["returns"]==["0x1008"] and not r["unresolved"]
    print("LIFECYCLE_CFG_ADJACENT_FUNCTION_EXCLUSION_PASS")
if sys.argv[1:]==["--selftest"]:selftest();raise SystemExit(0)
selftest()
b=Binary(load(Path(sys.argv[1]).resolve(strict=True),"btstack"))
assert b.word(0x11eab0)==0xeb0571b6 and b.word(0x125968)==0xeb055604 and b.word(0x11de34)==0xeb0574d1
# Stop entry 0x27b180 and adjacent start entry 0x27b190 are different
# direct targets. First bound explicitly excludes the start entry.
ranges={"osapi_stop":(0x27b180,0x27b180,0x27b190),"osapi_start":(0x27b190,0x27b190,0x27b380),"cleanup":(0x27a614,0x27a614,0x27a660),"service_stop":(0x125908,0x125908,0x125a90),"service_start":(0x11e9b0,0x11e9b0,0x11ed3c)}
res={k:walk(b.word,*v,b.labels) for k,v in ranges.items()}
# Examine direct callees from stop/cleanup, not arbitrary neighboring code.
# Tailcall or unresolved exits stay unresolved, not marked complete.
report={"scope":"ACTUAL_CONTROL_FLOW_WITH_EXPLICIT_BOUNDS_ONLY","routines":res,"original_sha256":"51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566","all_stop_paths_proven":False,"live_lease_proven":False,"installable":False}
Path(sys.argv[2]).write_text(json.dumps(report,indent=2)+"\n")
for k,v in res.items():
 print("LIFECYCLE_CFG",k,json.dumps({x:v[x] for x in ("entry","calls","returns","unresolved")}))
 if k in ("osapi_stop","cleanup"):
  for i in v["instructions"]:print("LIFECYCLE_INSTRUCTION",k,i["va"],i["mnemonic"],i["op_str"])
print("R105206_ORIGINAL_LIFECYCLE_CFG_READONLY_COMPLETE")
