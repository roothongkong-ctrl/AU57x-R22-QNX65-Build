#!/usr/bin/env python3
"""Target the remaining Congo build-table holes against exact original MU1005.
No address guessed into a callable table; no OEM bytes or binary patch output."""
import hashlib,json,pathlib,re,struct,subprocess,sys,tarfile
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
stock,archive,out=map(pathlib.Path,sys.argv[1:])
b=stock.read_bytes()
assert hashlib.sha256(b).hexdigest()=="51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
with tarfile.open(archive,"r:gz") as t:
    m=[m for m in t if m.isfile() and pathlib.PurePosixPath(m.name).name=="libbthook.so"]
    assert len(m)==1
    d=t.extractfile(m[0]).read()
assert hashlib.sha256(d).hexdigest()=="c862fe0d8bfbabdd804e39858616436481b19ce6620a2e915428620853280ad8"
def u32(data,off):return struct.unpack_from("<I",data,off)[0]
def segs(data):
    ph=u32(data,28);sz,n=struct.unpack_from("<HH",data,42)
    return [(v,v+f,o,flags) for k,o,v,_,f,_,flags,_ in (struct.unpack_from("<8I",data,ph+i*sz) for i in range(n)) if k==1]
def rd(data,va,n):
    for lo,hi,o,_ in segs(data):
        if lo<=va and va+n<=hi:return data[o+va-lo:o+va-lo+n]
    raise ValueError(f"unmapped {va:x}")
def word(a):return u32(rd(b,a,4),0)
md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
def dis(a,n):
    return [{"va":hex(i.address),"op":i.mnemonic+" "+i.op_str} for i in md.disasm(rd(b,a,n),a)]
def branch(w,a):
    if w>>28==15 or w&0x0e000000!=0x0a000000:return None
    q=w&0xffffff
    if q&0x800000:q-=0x1000000
    return a+8+4*q
# Only inspect the two ambiguous entries and the three original trace sites.
targets=[0x24352c,0x243530,0x1dbb40,0x1dbb4c]
calls={hex(a):[] for a in targets}
for lo,hi,o,flags in segs(b):
    if not flags&1:continue
    for off in range(0,hi-lo-3,4):
        w=u32(b,o+off);a=lo+off;dest=branch(w,a)
        if dest in targets and len(calls[hex(dest)])<24:
            calls[hex(dest)].append({"site":hex(a),"kind":"BL" if w&0x01000000 else "B","window":dis(a-16,24)})
# Relocation address comes from original ELF, not the donor offset or a guess.
rel=subprocess.check_output(["readelf","-rW",str(stock)],text=True)
acl=[line.strip() for line in rel.splitlines() if re.search(r"tracing.*(?:JOBS_STARTSERVICE|JOB.*STARTSERVICE|START_SERVICE)",line,re.I)]
sym=subprocess.check_output(["readelf","--dyn-syms","-W",str(stock)],text=True)
trace_symbols=[line.strip() for line in sym.splitlines() if re.search(r"tracing.*(?:JOBS_STARTSERVICE|JOB.*STARTSERVICE|START_SERVICE)",line,re.I)]
# Capture the known donor table span as labelled offsets, not a modified blob.
table=json.loads(pathlib.Path("integration/mu1005_vehicle_stage/congo_port_table.json").read_text())
known=[]
for row in table["entries"]:
    off=int(row["donor_file_offset"],16);dv=u32(d,off)
    entry={"name":row["name"],"donor_offset":hex(off),"donor_word":hex(dv),"target":row["stock_va"]}
    if row["stock_va"] and row["name"] not in ("startup_trace_slot","acl_trace_slot"):
        a=int(row["stock_va"],16);entry["target_entry_word"]=hex(word(a))
    known.append(entry)
assert next(x for x in known if x["name"]=="RF_RegisterServerChannel")["donor_word"]=="0x277b98"
assert next(x for x in known if x["name"]=="RF_RegisterServerChannel")["target_entry_word"]=="0xe92d4070"
result={"scope":"Exact missing interface evidence only; no callable ABI or firmware guard override",
        "candidate_windows":{hex(a):dis(a-12,64) for a in targets},"direct_branches":calls,
        "acl_trace_relocations":acl,"acl_trace_symbols":trace_symbols,
        "acl_trace_call_windows":{hex(a):dis(a-40,48) for a in (0x13f6c4,0x13f55c,0x13f1d8)},
        "table_fields":known,"installable":False,"runtime_verified":False}
out.write_text(json.dumps(result,indent=2)+"\n")
print(json.dumps(result))
