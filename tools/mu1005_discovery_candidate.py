#!/usr/bin/env python3
"""Private offline merge only. Output must not be installed or executed on a car."""
import argparse,struct,json,hashlib,subprocess
from pathlib import Path
from mu1005_lifecycle_observer import PIN,CODE,ENTRY,STATE,headers,align,need,branch
POINTS=((0x11ec98,0xe5c531b0,'mu_started'),(0x27a614,0xe5d03016,'mu_cleanup'),(0x12a6a0,0xe5c6040c,'mu_eir'))
def digest(x):return hashlib.sha256(x).hexdigest()
def read_graft(p):
    b=p.read_bytes()
    need(b[:6]==b'\x7fELF\x01\x01' and struct.unpack_from('<H',b,18)[0]==40,'not ARM32')
    off=struct.unpack_from('<I',b,32)[0];sz,n,st=struct.unpack_from('<HHH',b,46)
    need(sz==40 and off+n*sz<=len(b) and st<n,'bad sections')
    sh=[struct.unpack_from('<10I',b,off+i*40) for i in range(n)]
    strings=b[sh[st][4]:sh[st][4]+sh[st][5]]
    names=[strings[x[0]:].split(b'\0',1)[0] for x in sh]
    need(names.count(b'.graft')==1,'missing graft')
    x=sh[names.index(b'.graft')]
    need(x[3]==ENTRY and x[4]+x[5]<=len(b) and x[5]>0,'graft placement')
    need(all(not (z[2]&2) or nm==b'.graft' or z[5]==0 for nm,z in zip(names,sh)),'unexpected allocated section')
    need('There are no relocations' in subprocess.check_output(['readelf','-rW',str(p)],text=True),'unresolved relocation')
    symbols={}
    for line in subprocess.check_output(['readelf','-sW',str(p)],text=True).splitlines():
        a=line.split()
        if a and a[-1] in {q[2] for q in POINTS}:
            need(a[-1] not in symbols,'duplicate hook');symbols[a[-1]]=int(a[1],16)
    need(len(symbols)==3 and symbols['mu_started']==ENTRY and all(ENTRY<=v<ENTRY+x[5] and v%4==0 for v in symbols.values()),'hook symbols')
    return b[x[4]:x[4]+x[5]],symbols
def build(source,graft,out):
    b=source.read_bytes();need(digest(b)==PIN,'wrong original')
    old=headers(b);need([x[0] for x in old]==[6,3,1,1,2],'unexpected OEM PHDR')
    need(all(x[2]+x[5]<=CODE for x in old if x[0]==1),'OEM overlaps')
    for a,w,_ in POINTS:need(struct.unpack_from('<I',b,a-0x100000)[0]==w,'wrong hook word')
    # The actual MU1005 SDK contains native initial credits 4 and 7.
    for a,w in [(0x24dca8,0xe3a02004),(0x24dcac,0xeb00a759),(0x2464a0,0xe3a02007),(0x2464b0,0xeb00c558)]:
        need(struct.unpack_from('<I',b,a-0x100000)[0]==w,'credit evidence differs')
    code,symbols=read_graft(graft)
    need(CODE+256+len(code)<STATE,'graft overlaps state')
    off=align(len(b),4096);payload=bytearray(256)+code;stateoff=align(off+len(payload),4096)
    new=[x[:] for x in old];new[0]=[6,off,CODE,CODE,224,224,4,4]
    new.extend([[1,off,CODE,CODE,len(payload),len(payload),5,4096],[1,stateoff,STATE,STATE,0,0x20000,6,4096]])
    payload[:224]=b''.join(struct.pack('<8I',*v) for v in new)
    image=bytearray(b);struct.pack_into('<I',image,28,off);struct.pack_into('<H',image,44,7)
    allowed=set(range(28,32))|set(range(44,46))
    for va,w,s in POINTS:
        o=va-0x100000;struct.pack_into('<I',image,o,branch(va,symbols[s]));allowed.update(range(o,o+4))
    need(all(i in allowed for i,(a,c) in enumerate(zip(b,image)) if a!=c),'unexpected change')
    image.extend(bytes(off-len(image)));image.extend(payload);image.extend(bytes(stateoff-len(image)))
    need(headers(image)==new and source.read_bytes()==b,'layout/original changed')
    out.mkdir(parents=True,exist_ok=False)
    (out/'btstack.discovery-NO-INSTALL.bin').write_bytes(image)
    report=dict(original_sha256=PIN,candidate_sha256=digest(image),graft_sha256=digest(graft.read_bytes()),installable=False,vehicle_verified=False,phase='DISCOVERY_ONLY',accept_rfcomm=False,original_loads_unchanged=True,hooks={hex(a):hex(symbols[s]) for a,w,s in POINTS},initial_rx_credits=4,credits_profile_iap2_verified=False,original_runtime_verified=False)
    (out/'STATUS.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--stock',type=Path,required=True);p.add_argument('--graft',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();build(a.stock,a.graft,a.out)
