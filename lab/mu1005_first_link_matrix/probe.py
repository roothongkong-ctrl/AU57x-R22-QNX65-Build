#!/usr/bin/env python3
"""15 independent targeted static checks. No firmware writes, no runtime success claims."""
import hashlib,io,json,re,struct,sys
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from elftools.elf.relocation import RelocationSection
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_OP_IMM
from callchain import main as chain_main,selftest as chain_test,decode
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_iap_consumer"))
from trace import trace
PINS={
"btstack":"51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566",
"bluetooth":"7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272",
"smartphone_integrator":"2c61e7a17d5c77df2f97023284b948b66dc4052010acad74be9bfa63b339de0b",
"ipod-drvr-iap2.so":"42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf",
"dio_manager":"e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c"}
LANES={
"06-bt-service-base":("btstack","vtable",0x2be7a0,72),
"06-bt-start-job":("btstack","vtable",0x2bec90,56),
"06-bt-sdp-job":("btstack","vtable",0x2be908,56),
"07-config-callchain":("bluetooth","chain",0x1562f4,0),
"07-iap-enabled-table":("bluetooth","table","iapEnabled",0),
"07-iap-service-table":("bluetooth","table","SERVICETYPE_IAP2",0),
"08-si-prepare":("smartphone_integrator","symbol","requestPrepareConnect",0),
"08-si-connection":("smartphone_integrator","symbol","reportConnectionEstablished",0),
"09-driver-component":("ipod-drvr-iap2.so","marker","BluetoothTransportComponent",0),
"09-driver-component-id":("ipod-drvr-iap2.so","marker","BluetoothTransportComponentIdentifier",0),
"09-driver-support":("ipod-drvr-iap2.so","marker","TransportSupportsiAP2Connection",0),
"10-hid-up":("ipod-drvr-iap2.so","symbol","hid_remote_volup",0),
"10-hid-down":("ipod-drvr-iap2.so","symbol","hid_remote_voldown",0),
"10-driver-bt-link":("ipod-drvr-iap2.so","marker","BluetoothiAP2Link",0),
"10-dio-ipod-connect":("dio_manager","symbol","iap2_connect",0)}
def load(root,name):
    for p in sorted(root.rglob(name)):
        if p.is_file() and p.resolve().is_relative_to(root):
            d=p.read_bytes()
            if hashlib.sha256(d).hexdigest()==PINS[name]:return d
    raise ValueError("missing exact original "+name)
def imm(w):
    n=w&255;s=((w>>8)&15)*2
    return ((n>>s)|(n<<(32-s)))&0xffffffff if s else n
def prel_branch(w,va):
    if w&0x0f000000!=0x0b000000 or w>>28==15:return None
    d=(w&0xffffff)<<2
    if d&0x2000000:d-=0x4000000
    return (va+8+d)&0xffffffff
class Binary:
    def __init__(self,d):
        self.d=d;self.e=ELFFile(io.BytesIO(d))
        assert self.e.elfclass==32 and self.e.little_endian and self.e["e_machine"]=="EM_ARM"
        self.loads=[(int(s["p_offset"]),int(s["p_vaddr"]),int(s["p_filesz"])) for s in self.e.iter_segments() if s["p_type"]=="PT_LOAD"]
        t=self.e.get_section_by_name(".text");assert t is not None
        self.code=t.data();self.va=int(t["sh_addr"]);self.names={};self.reloc={};self.plt={};self.funcs=[]
        for sec in self.e.iter_sections():
            if isinstance(sec,SymbolTableSection):
                for s in sec.iter_symbols():
                    v=int(s["st_value"]);n=int(s["st_size"])
                    if v and s.name and s["st_shndx"]!="SHN_UNDEF":
                        self.names.setdefault(v,set()).add(s.name)
                        if n and s["st_info"]["type"]=="STT_FUNC":self.funcs.append((v,n,s.name))
            if isinstance(sec,RelocationSection):
                sym=self.e.get_section(sec["sh_link"])
                for r in sec.iter_relocations():
                    s=sym.get_symbol(r["r_info_sym"]) if r["r_info_sym"] else None
                    self.reloc.setdefault(int(r["r_offset"]),[]).append({"type":int(r["r_info_type"]),"symbol":s.name if s else None,"symbol_va":int(s["st_value"]) if s else 0,"addend":int(r["r_addend"]) if r.is_RELA() else None})
        p=self.e.get_section_by_name(".plt")
        if p is not None:
            pd=p.data();pv=int(p["sh_addr"])
            for o in range(0,len(pd)-11,4):
                a,b,c=struct.unpack_from("<III",pd,o)
                if a&0xfffff000!=0xe28fc000 or b&0xfffff000!=0xe28cc000 or c&0xfffff000!=0xe5bcf000:continue
                slot=(pv+o+8+imm(a)+imm(b)+(c&4095))&0xffffffff
                for r in self.reloc.get(slot,[]):
                    if r["type"]==22 and r["symbol"]:self.plt[pv+o]=r["symbol"]
    def word(self,a):
        for off,v,n in self.loads:
            if v<=a and a+4<=v+n:return struct.unpack_from("<I",self.d,off+a-v)[0]
        return None
    def labels(self,a):
        return sorted(self.names.get(a,set())|({self.plt[a]+"@plt"} if a in self.plt else set()))
    def owner(self,a):
        f=sorted((n,s,v) for v,n,s in self.funcs if v<=a<v+n)
        return {"name":f[0][1],"va":hex(f[0][2])} if f else None
    def marker(self,name,with_code):
        needle=name.encode()+b"\0";targets=set();pos=0
        while (pos:=self.d.find(needle,pos))!=-1:
            for off,v,n in self.loads:
                if off<=pos and pos+len(needle)<=off+n:targets.add(v+pos-off)
            pos+=1
        refs=[]
        for s in self.e.iter_sections():
            if not s["sh_flags"]&2 or s["sh_flags"]&4 or s["sh_type"]=="SHT_NOBITS":continue
            d=s.data();v=int(s["sh_addr"])
            for off in range(0,len(d)-3,4):
                val=struct.unpack_from("<I",d,off)[0];addr=v+off
                rels=self.reloc.get(addr,[])
                resolved={val}|{(r["symbol_va"]+(r["addend"] if r["addend"] is not None else val))&0xffffffff for r in rels}
                for hit in resolved&targets:
                    refs.append({"va":hex(addr),"target":hex(hit),"section":s.name,"relocations":rels})
        # Existing bounded PIC decoder also checks references to the discovered tables.
        # New marker lanes target actual metadata and pointer slots, not old direct-only scan.
        ptrs={int(r["va"],16):"pointer_slot" for r in refs}
        mapped={a:name for a in targets} if with_code else {}
        mapped.update(ptrs)
        hits=trace(self.code,self.va,self.word,mapped) if mapped else []
        return {"marker":name,"string_vas":[hex(a) for a in sorted(targets)],"pointer_slots":refs[:100],"pointer_slot_count":len(refs),"code_reference_candidates":hits[:100],"code_reference_count":len(hits)}
    def vtable(self,a,n):
        rows=[]
        for v in range(a,a+n,4):
            w=self.word(v)
            if w is None:raise ValueError("vtable not file-backed")
            rels=self.reloc.get(v,[])
            cand={w}|{(r["symbol_va"]+(r["addend"] if r["addend"] is not None else w))&0xffffffff for r in rels}
            rows.append({"slot":v-a,"relocations":rels,"function_candidates":[{"va":hex(x),"labels":self.labels(x),"owner":self.owner(x)} for x in sorted(cand) if self.va<=x<self.va+len(self.code)]})
        return {"table_va":hex(a),"table_labels":self.labels(a),"slots":rows}
    def symbol(self,needle):
        targets={v for v,n in self.names.items() if any(needle in s for s in n)}
        targets.update(v for v,n in self.plt.items() if needle in n)
        hits=[]
        for off in range(0,len(self.code)-3,4):
            a=self.va+off;v=prel_branch(struct.unpack_from("<I",self.code,off)[0],a)
            if v not in targets:continue
            # Surrounding calls and branch decisions provide context, not a proved ABI.
            lo=max(0,off-48);hi=min(len(self.code),off+64)
            calls,branches=decode(self.code[lo:hi],self.va+lo,{v:self.labels(v) for v in targets})
            hits.append({"va":hex(a),"target":hex(v),"labels":self.labels(v),"owner":self.owner(a),"nearby_calls":calls,"nearby_branches":branches})
        return {"symbol_query":needle,"target_candidates":[{"va":hex(v),"labels":self.labels(v)} for v in sorted(targets)][:100],"direct_call_candidates":hits[:100],"direct_call_count":len(hits)}
def selftest():
    assert prel_branch(0xeb000002,0x1000)==0x1010
    assert prel_branch(0xeafffffe,0x1000) is None
    assert len(LANES)==15
    chain_test()
    print("FIRST_LINK_MATRIX_SELFTEST_OK")
def main(lane,root,out):
    name,mode,arg,n=LANES[lane]
    if mode=="chain":
        chain_main(root,out)
        result=json.loads(out.read_text());found=bool(result["nearby_calls"] or result["calls"])
    else:
        b=Binary(load(root,name))
        if mode=="vtable":
            result=b.vtable(arg,n);found=any(x["function_candidates"] for x in result["slots"])
        elif mode=="symbol":
            result=b.symbol(arg);found=bool(result["direct_call_count"])
        else:
            result=b.marker(arg,mode=="marker");found=bool(result["pointer_slot_count"] or result["code_reference_count"])
    report={"lane":lane,"target":name,"sha256":PINS[name],"scope":"STATIC_CANDIDATES_ONLY","candidate_found":found,"result":result,"wireless_verified":False,"runtime_verified":False,"installable":False}
    out.write_text(json.dumps(report,indent=2)+"\n")
    print("R105144_LANE",lane,"CANDIDATE_FOUND" if found else "NO_CANDIDATE")
    print(json.dumps(report,sort_keys=True))
    if not found:raise SystemExit(3)
if __name__=="__main__":
    if sys.argv[1:]==["--selftest"]:selftest()
    else:main(sys.argv[1],Path(sys.argv[2]).resolve(strict=True),Path(sys.argv[3]))
