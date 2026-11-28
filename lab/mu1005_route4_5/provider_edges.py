#!/usr/bin/env python3
"""Find original receiver lifecycle callers and driver callback producers.
Scope: pinned app70 only; candidate offsets/labels, no binary patch.
"""
import hashlib,json,re,struct,sys
from pathlib import Path
from lifecycle_cfg import md
from probe import Binary,load,prel_branch
from capstone.arm import ARM_OP_MEM,ARM_OP_REG,ARM_REG_PC

def direct_calls(b, target):
    for o in range(0,len(b.code)-3,4):
        a=b.va+o
        v=prel_branch(struct.unpack_from("<I",b.code,o)[0],a)
        if v in target:
            yield a,v

def nearby(b,a,pre=32,post=20):
    rows=[]
    for p in range(a-pre,a+post+4,4):
        v=b.word(p)
        if v is None: continue
        i=next(md.disasm(v.to_bytes(4,"little"),p,count=1),None)
        if i is not None:
            rows.append({"va":hex(p),"op":i.mnemonic,"args":i.op_str})
    return rows

def dio(b):
    # Known creation/delegate/control path from R105.212. Look for OTHER
    # consumers of the original receiver and dispatch queue, in particular
    # teardown and release, rather than re-decoding the known create call.
    pattern=re.compile(r"AirPlayReceiverServer(GetDispatchQueue|SetDelegate|Control|Stop|Delete|Release|Finalize)|dispatch_(sync|async|release)|CFRelease")
    targets={a for a,n in b.plt.items() if pattern.fullmatch(n)}
    rows=[]
    for a,v in direct_calls(b,targets):
        name=b.plt[v]
        owner=b.owner(a)
        if name.startswith("AirPlayReceiverServer") or (owner and re.search(r"DIOManager|AirPlay",owner["name"],re.I)):
            rows.append({"site":hex(a),"import":name,"owner":owner,
                         "window":nearby(b,a)})
    # Explicit function symbols and relocations for receiver global and DIO
    # lifecycle are evidence of linkage, NOT proof of runtime lease ownership.
    reloc=[{"slot":hex(a),"entries":[r for r in rs if r["symbol"] and
            re.search(r"gAirPlayReceiverServer|CDIOManager.*(D[012]E|stop|close|shutdown)",r["symbol"],re.I)]}
           for a,rs in b.reloc.items()]
    return {"lifecycle_calls":rows,"relocations":[x for x in reloc if x["entries"]],
            "direct_only":True}

def transport(b):
    # Prior R105.212 verified callback offsets 0x28/0x2c at dispatch.
    # Find functions that WRITE both slots against the same base register
    # inside symbol-bounded code. This distinguishes candidate constructors
    # from mere callback readers; it does not establish a base pointer type.
    rows=[]
    seen=set()
    for a,n,name in b.funcs:
        if (a,n) in seen or n<8 or n>65536 or a%4:continue
        seen.add((a,n))
        if not (b.va<=a and a+n<=b.va+len(b.code)):continue
        writes={}
        for i in md.disasm(b.code[a-b.va:a-b.va+n],a):
            if not i.mnemonic.startswith("str") or len(i.operands)<2:continue
            op=i.operands[1]
            if op.type!=ARM_OP_MEM or op.mem.base==ARM_REG_PC or op.mem.index or op.mem.disp not in (0x28,0x2c):continue
            writes.setdefault(op.mem.base,{}).setdefault(op.mem.disp,[]).append(i.address)
        for base,slots in writes.items():
            if 0x28 not in slots or 0x2c not in slots:continue
            for x in slots[0x28]:
                for y in slots[0x2c]:
                    if abs(x-y)<=64:
                        rows.append({"function":name,"entry":hex(a),"base_register":md.reg_name(base),
                                     "send_slot_write":hex(x),"receive_slot_write":hex(y),
                                     "window":nearby(b,min(x,y),20,abs(x-y)+12)})
    return {"paired_slot_store_candidates":rows[:200],"candidate_count":len(rows),
            "symbol_bounded":True,"actual_descriptor_identity_proven":False}

if __name__=="__main__":
    lane,root,out=sys.argv[1:]
    name={"dio-owner":"dio_manager","transport-provider":"ipod-drvr-iap2.so"}[lane]
    b=Binary(load(Path(root).resolve(strict=True),name))
    result=dio(b) if lane=="dio-owner" else transport(b)
    report={"lane":lane,"sha256":hashlib.sha256(b.d).hexdigest(),"result":result,
            "runtime_verified":False,"installable":False}
    Path(out).write_text(json.dumps(report,indent=2)+"\n")
    print("R105215_PROVIDER_CANDIDATES",lane,json.dumps(result)[:60000])
