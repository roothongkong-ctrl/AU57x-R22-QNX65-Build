#!/usr/bin/env python3
"""Read-only private MU1005 dependency extraction. Never upload OEM bytes."""
from pathlib import Path
import argparse,hashlib,json,re,struct,subprocess
def sha(b):return hashlib.sha256(b).hexdigest()
def need(ok,msg):
    if not ok:raise ValueError(msg)
def inventory(raw,app,efs,out):
    b=raw.read_bytes()
    need(sha(b)=="43aa81946b8bd513eab0d804a99a85e6ff99f3b36b9a39b2ee68ae1625b9140f","wrong original stage2")
    need(b[:8]==b'imagefs\x04',"wrong imagefs")
    length,dsize,doff=struct.unpack_from('<III',b,8)
    need(length==len(b) and doff==92 and doff+dsize<len(b),"directory extent")
    entries={};pos=doff
    while pos+2<=doff+dsize:
        n=struct.unpack_from('<H',b,pos)[0]
        if not n:break
        need(n>=28 and pos+n<=doff+dsize,"entry extent")
        mode=struct.unpack_from('<I',b,pos+8)[0]
        if mode&0xf000==0x8000:
            off,size=struct.unpack_from('<II',b,pos+24)
            name=b[pos+32:pos+n].split(b'\0')[0].decode()
            need(off+size<=len(b) and not name.startswith('/') and '..' not in Path(name).parts and name not in entries,"entry")
            entries[name]=(off,size)
        pos+=n
    need(len(entries)>100,"unexpected directory")
    found={};missing=set();edges={};pending=[]
    pins={"btstack":"51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566","bluetooth":"7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272","connectivity_launcher":"08896149dcdcc2bc5a6b22ce78682ad5949fbeb8fdc0fb1249162861e40914b8","servicemgrmibhigh":"719e34dc990b4ef2ab87a90e3d84de3f2188189df2efa4f07b2433d312eb11d1","persistenceTNG_mibhigh":"706561a8b005a2896ef7e1c99ccc69643de16f1ad3f119213f325edf604adfb7"}
    for name,pin in pins.items():
        src=app/name;v=src.read_bytes();need(sha(v)==pin,"wrong original "+name)
        found[name]=dict(source="private original",sha256=pin,size=len(v));pending.append((name,src))
    eb=efs.read_bytes();need(sha(eb)=="96fbdddae01ec4e785104e9516f0a84706ff6c2b8c912c2c33ad066988bd8244","wrong EFS")
    configs={}
    for name,a,z,pin in [
        ("connectivity.json",0x84e80,0x8707b,"56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470"),
        ("dsistartup.json",0x19640,0x1e1e8,"c0fe47842c83ae88a5de608dbc65bcfc7b47df513462775566fe2467c479f2f4")]:
        v=eb[a:z];need(sha(v)==pin,"wrong config extent")
        configs[name]=(v,json.loads(re.sub(rb'(?m)^[ \t]*#[^\r\n]*',b'',v)))
    out.mkdir(parents=True,exist_ok=False)
    libs={}
    for name,(off,size) in entries.items():
        if b[off:off+4]==b'\x7fELF':libs.setdefault(Path(name).name,[]).append(name)
    while pending:
        name,src=pending.pop(0)
        deps=re.findall(r'Shared library: \[(.*?)\]',subprocess.check_output(['readelf','-dW',str(src)],text=True,stderr=subprocess.DEVNULL))
        edges[name]=deps
        for dep in deps:
            if dep in found or dep in missing:continue
            if len(libs.get(dep,[]))!=1:missing.add(dep);continue
            n=libs[dep][0];off,size=entries[n];v=b[off:off+size]
            dest=out/dep;dest.write_bytes(v)
            found[dep]=dict(source='stage2:'+n,sha256=sha(v),size=size);pending.append((dep,dest))
    for name,(v,_) in configs.items():(out/name).write_bytes(v)
    c=configs['connectivity.json'][1];s=configs['dsistartup.json'][1]
    original_runmode=[x for a in s['on_startup_normal'] if a.get('cmd')=='fork' for x in a['commands'] if x.get('cmd')=='set_app_runmode' and x.get('app')=='connectivity_launcher']
    report=dict(scope='PRIVATE_STATIC_DEPENDENCY_INVENTORY_NOT_RUNTIME',original_runtime=False,installable=False,stage2_sha256=sha(b),found=found,missing=sorted(missing),dependencies=edges,configs={n:sha(v) for n,(v,_) in configs.items()},init_runmode=c['launcher']['initRunMode'],later_runmode=original_runmode[0]['apprunmode'],wait_for_ooc=s['wait_for_ooc'],framework_complete=False)
    (out/'inventory.json').write_text(json.dumps(report,indent=2)+'\n')
    return report
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['raw','app','efs','out']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();print(json.dumps(inventory(a.raw,a.app,a.efs,a.out),indent=2))
