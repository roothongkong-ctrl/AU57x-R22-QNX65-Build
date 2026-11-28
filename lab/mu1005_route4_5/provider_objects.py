#!/usr/bin/env python3
"""Follow concrete R105.215 evidence, not another paired-offset search.
No runtime or install assertion, no firmware changes."""
import hashlib,json,sys
from pathlib import Path
from lifecycle_cfg import md,walk
from probe import Binary,load
from provider_edges import direct_calls,nearby
from capstone.arm import ARM_OP_MEM,ARM_REG_PC,ARM_REG_SP

def stop_job(b):
    name="_ZTVN3dio39CDIOManagerControllerJob_StopControllerE"
    symbols=[]
    for sec in b.e.iter_sections():
        if sec["sh_type"] not in ("SHT_SYMTAB","SHT_DYNSYM"):continue
        for s in sec.iter_symbols():
            if s.name==name and s["st_shndx"]!="SHN_UNDEF" and s["st_size"]:
                symbols.append((int(s["st_value"]),int(s["st_size"])))
    symbols=sorted(set(symbols))
    tables=[]
    for a,n in symbols:
        if n<8 or n>512 or n%4:continue
        table=b.vtable(a,n)
        functions=[]
        seen=set()
        for row in table["slots"]:
            for f in row["function_candidates"]:
                va=int(f["va"],16)
                if va in seen:continue
                seen.add(va)
                bounds=sorted(set((p,p+size) for p,size,_ in b.funcs if p==va and 0<size<=8192))
                functions.append({"candidate":f,"bounds":bounds,
                  "cfg":walk(b.word,va,*bounds[0],b.labels) if len(bounds)==1 else None,
                  "direct_callers":[hex(x) for x,_ in direct_calls(b,{va})]})
        tables.append({"table":table,"functions":functions})
    return {"vtable_symbol":name,"defined_symbols":symbols,"tables":tables,
      "runtime_owner_proven":False}

def driver(b):
    # R105.212 actual dispatch: transport_receive uses [arg+0x14]+0x40,
    # send uses [r4+0x40]; both then load a callback from that table.
    for site in (0x21128,0x211e0):
        i=next(md.disasm(b.word(site).to_bytes(4,"little"),site,count=1))
        assert i.mnemonic=="ldr" and i.operands[1].mem.disp==0x40
    names=("transport_receive","transport_send_pkt","transport_recv_pkt")
    targets={a for a,n,s in b.funcs if s in names}
    callers=[{"site":hex(a),"target":hex(v),"owner":b.owner(a),
              "window":nearby(b,a,20,12)} for a,v in direct_calls(b,targets)]
    writes=[]
    seen=set()
    for a,n,name in b.funcs:
        if (a,n) in seen or n<8 or n>65536 or a%4:continue
        if not (b.va<=a and a+n<=b.va+len(b.code)):continue
        seen.add((a,n))
        for i in md.disasm(b.code[a-b.va:a-b.va+n],a):
            if i.mnemonic not in ("str","streq","strne") or len(i.operands)<2:continue
            op=i.operands[1]
            if op.type!=ARM_OP_MEM or op.mem.base in (ARM_REG_PC,ARM_REG_SP) or op.mem.index or op.mem.disp!=0x40:continue
            writes.append({"function":name,"entry":hex(a),"site":hex(i.address),
                           "window":nearby(b,i.address,20,12)})
    return {"dispatch_callers":callers,"object_pointer_store_count":len(writes),
      "object_pointer_store_candidates":writes[:250],"object_type_proven":False}

if __name__=="__main__":
    lane,root,out=sys.argv[1:]
    name={"dio-stop-job":"dio_manager","driver-object":"ipod-drvr-iap2.so"}[lane]
    b=Binary(load(Path(root).resolve(strict=True),name))
    result=stop_job(b) if lane=="dio-stop-job" else driver(b)
    report={"lane":lane,"sha256":hashlib.sha256(b.d).hexdigest(),"result":result,
            "runtime_verified":False,"installable":False}
    Path(out).write_text(json.dumps(report,indent=2)+"\n")
    print("R105216_OBJECT_EVIDENCE",lane,json.dumps(result)[:60000])
