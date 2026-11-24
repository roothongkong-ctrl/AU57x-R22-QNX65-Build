#!/usr/bin/env python3
"""Read-only SI candidate map. No patch generation or runtime permission."""
import argparse, hashlib, json, struct
from pathlib import Path

SOURCE_SHA = "606236a81c7c4dafda6fdd08fe6eb27c43252796784da25d9760a7d552ee2af3"
TARGET_SHA = "2c61e7a17d5c77df2f97023284b948b66dc4052010acad74be9bfa63b339de0b"
POINTS = {
    "publish":0x183678, "string_set":0x115274, "record_ctor":0x169850,
    "upsert":0x16eef4, "update":0x16ec90, "string_dtor":0x1153b8,
    "remove":0x169984, "activate":0x165390, "get_state":0x165198,
    "asi_ctor_state":0x1274b0, "asi_ctor_watchdog":0x127440,
    "asi_ctor_flush":0x1273cc, "asi_ctor_teardown":0x12735c,
    "asi_ctor_restart":0x1272f4, "asi_convert":0x170cc4,
    "event":0x12584c, "job_start":0x17f2b8, "job_alloc":0x1293d8,
    "job_enqueue":0x13f4f4, "job_delete":0x129050,
    "prepare_hook":0x1800b0, "cleanup_hook":0x17fc48,
    "dispatch_hook":0x128dbc, "transport_hook":0x169518,
    "controller_dispatch":0x1653c0
}

class Elf:
    def __init__(self, path, expected):
        self.data = Path(path).read_bytes()
        self.sha = hashlib.sha256(self.data).hexdigest()
        if self.sha != expected:
            raise ValueError("unrecognized input SHA256: " + self.sha)
        if self.data[:6] != b"\x7fELF\x01\x01":
            raise ValueError("not ELF32 little-endian")
        h = struct.unpack_from("<16sHHIIIIIHHHHHH", self.data)
        if h[1:3] != (2,40) or h[9] != 32 or h[11] != 40:
            raise ValueError("not expected ARM executable")
        self.loads = []
        for i in range(h[10]):
            t, off, va, pa, fs, ms, flags, align = struct.unpack_from("<8I",self.data,h[5]+i*32)
            if t == 1 and flags & 1:
                self.block(off,fs)
                self.loads.append((va,off,fs))
        sections = [struct.unpack_from("<10I",self.data,h[6]+i*40) for i in range(h[12])]
        self.by_name, self.by_addr = {}, {}
        for sec in sections:
            if sec[1] not in (2,11):
                continue
            if sec[9]!=16 or sec[5]%16 or sec[6]>=len(sections):
                raise ValueError("unexpected symbol table")
            st=sections[sec[6]]
            strings=self.block(st[4],st[5])
            table=self.block(sec[4],sec[5])
            for off in range(0,len(table),16):
                name, va, size, info, other, shndx=struct.unpack_from("<IIIBBH",table,off)
                if not shndx or info&15!=2 or not va or not name:
                    continue
                if name>=len(strings) or strings.find(b"\0",name)<0:
                    raise ValueError("bad symbol name")
                n=strings[name:strings.index(b"\0",name)].decode("ascii","replace")
                if not n or not self.prefix(va):
                    continue
                self.by_name.setdefault(n,set()).add((va,size))
                self.by_addr.setdefault(va,set()).add((n,size))

    def block(self,off,n):
        if off<0 or n<0 or off+n>len(self.data):
            raise ValueError("truncated ELF range")
        return self.data[off:off+n]

    def code(self,va,n):
        if va&3 or n<=0:
            return None
        matches=[self.block(off+va-base,n) for base,off,fs in self.loads
                 if base<=va and va+n<=base+fs]
        return matches[0] if len(matches)==1 else None

    def prefix(self,va):
        # ARM only. Thumb and non-file-backed entries are not mapped.
        if va&3:
            return None
        matches=[self.block(off+va-base,12) for base,off,fs in self.loads
                 if base<=va and va+12<=base+fs]
        return matches[0] if len(matches)==1 else None

def instruction_mask(word):
    # Only mask PC-relative immediate fields. Preserve conditions, registers,
    # instruction classes and ordinary object-offset immediates.
    if (word & 0x0e000000)==0x0a000000 and (word>>28)!=15:
        return 0xff000000
    if (word & 0x0e0f0000)==0x040f0000 and (word>>28)!=15:
        return 0xff7ff000
    return 0xffffffff

def masked_signature(data):
    words=struct.unpack("<"+"I"*(len(data)//4),data)
    return [(w&instruction_mask(w),instruction_mask(w)) for w in words]

def code_candidates(src,dst,va):
    # Two disjoint 32-byte blocks; do not choose based on a short prologue.
    code=src.code(va,64)
    if not code:
        return {"assessment":"insufficient_file_backed_bytes","addresses":[]}
    sig=masked_signature(code)
    fixed=sum(mask==0xffffffff for _,mask in sig)
    if fixed<10:
        return {"assessment":"insufficient_fixed_words","addresses":[],"fixed_words":fixed}
    # Use a fixed word as an index to avoid scanning every 64-byte window.
    indexed=[i for i,(_,m) in enumerate(sig) if m==0xffffffff]
    idx=indexed[len(indexed)//2]
    needle=struct.pack("<I",sig[idx][0])
    hits=[]
    for base,off,fs in dst.loads:
        buf=dst.block(off,fs)
        start=0
        while True:
            pos=buf.find(needle,start)
            if pos<0:break
            start=pos+1
            p=pos-4*idx
            if p<0 or p+64>len(buf) or (base+p)&3:continue
            vals=struct.unpack_from("<16I",buf,p)
            if all((w&m)==v for w,(v,m) in zip(vals,sig)):
                hits.append(base+p)
    hits=sorted(set(hits))
    return {"assessment":"unique_masked_64byte_candidate" if len(hits)==1 else
            "ambiguous" if hits else "not_found",
            "fixed_words":fixed,"addresses":[hex(x) for x in hits],
            "exact_64byte_match":[hex(x) for x in hits if dst.code(x,64)==code]}

def compare(src,dst):
    rows=[]
    for role,va in POINTS.items():
        prefix=src.prefix(va)
        aliases=sorted(src.by_addr.get(va,set()))
        candidates={}
        for name,size in aliases:
            for tva,tsize in dst.by_name.get(name,set()):
                item=candidates.setdefault(tva,{"address":hex(tva),"matches":[]})
                item["matches"].append({"symbol":name,"source_size":size,"target_size":tsize,
                     "same_nonzero_size":size>0 and size==tsize,
                     "same_prefix":prefix is not None and prefix==dst.prefix(tva)})
        items=[candidates[k] for k in sorted(candidates)]
        strong=[x for x in items if any(m["same_nonzero_size"] and m["same_prefix"] for m in x["matches"])]
        rows.append({"role":role,"source_va":hex(va),"source_prefix":prefix.hex() if prefix else None,
          "source_symbols":[n for n,_ in aliases],"candidates":items,
          "instruction_candidates":code_candidates(src,dst,va),
          "assessment":"unique_named_prefix_candidate" if len(strong)==1 and len(items)==1 else
            ("no_source_symbol" if not aliases else "unresolved_or_changed")})
    return {"source_sha256":src.sha,"target_sha256":dst.sha,"points":rows,
       "mapped_candidates":sum(r["assessment"]=="unique_named_prefix_candidate" for r in rows),
       "unique_instruction_candidates":sum(r["instruction_candidates"]["assessment"]=="unique_masked_64byte_candidate" for r in rows),
       "runtime_approved":False,"installable":False,
       "unverified":["function semantics","data structure offsets","global singleton addresses",
                     "vtable slots","call sites","hook relocation","native ownership"]}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source",required=True,type=Path)
    p.add_argument("--target",type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--self-review",action="store_true",help="donor vs donor only, never MU1005 evidence")
    a=p.parse_args()
    if a.self_review==bool(a.target):
        p.error("choose exactly one of --target or --self-review")
    if a.out.exists():
        p.error("refuse to overwrite an existing file")
    src=Elf(a.source,SOURCE_SHA)
    dst=src if a.self_review else Elf(a.target,TARGET_SHA)
    report=compare(src,dst)
    report["scope"]="DONOR_SELF_REVIEW_ONLY" if a.self_review else "MU1005_PRIVATE_ELF_REVIEW_ONLY"
    if a.self_review:
        assert all(r["source_prefix"] is not None for r in report["points"])
        for row in report["points"]:
            c=row["instruction_candidates"]
            if c["assessment"]=="unique_masked_64byte_candidate":
                assert c["addresses"]==[row["source_va"]],row["role"]
        assert report["unique_instruction_candidates"]>0
    a.out.write_text(json.dumps(report,indent=2)+"\n")
    print(report["scope"],"named candidate count",report["mapped_candidates"],"/",len(POINTS),"instruction candidate count",report["unique_instruction_candidates"],"/",len(POINTS),"NO INSTALL")
if __name__=="__main__":
    main()
