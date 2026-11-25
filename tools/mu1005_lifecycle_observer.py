#!/usr/bin/env python3
"""Offline MU1005 startup/cleanup observer. Never install or execute this OEM copy."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,tempfile
PIN='51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566'
CODE=0x510000; ENTRY=CODE+0x100; STATE=0x520000
POINTS=((0x11ec98,0xe5c531b0,'mu_started'),(0x27a614,0xe5d03016,'mu_cleanup'))
ASM=r'''
.syntax unified
.arm
.text
.global mu_started, mu_cleanup
mu_started:
    strb r3,[r5,#0x1b0]
    push {r0-r3,r12,lr}
    ldr r0,=0x520000
    str r5,[r0,#4]
    ldr r1,[r0,#12]
    add r1,r1,#1
    str r1,[r0,#12]
    dmb ish
    mov r1,#1
    str r1,[r0]
    dmb ish
    pop {r0-r3,r12,lr}
    ldr pc,=0x11ec9c
mu_cleanup:
    ldrb r3,[r0,#0x16]
    push {r0-r3,r12,lr}
    ldr r1,=0x520000
    mov r2,#0
    str r2,[r1]
    dmb ish
    str r0,[r1,#8]
    str r3,[r1,#20]
    ldr r2,[r1,#16]
    add r2,r2,#1
    str r2,[r1,#16]
    dmb ish
    pop {r0-r3,r12,lr}
    ldr pc,=0x27a618
.ltorg
'''
def digest(b):return hashlib.sha256(b).hexdigest()
def need(ok,msg):
    if not ok:raise ValueError(msg)
def align(x,n):return (x+n-1)&-n
def headers(b):
    off=struct.unpack_from('<I',b,28)[0];size,n=struct.unpack_from('<HH',b,42)
    need(size==32 and off+n*size<=len(b),'bad phdr')
    return [list(struct.unpack_from('<8I',b,off+32*i)) for i in range(n)]
def branch(a,b):
    x=b-a-8;need(x%4==0 and -(1<<25)<=x<(1<<25),'branch range')
    return 0xea000000|((x>>2)&0xffffff)
def build(source,out):
    b=source.read_bytes();need(digest(b)==PIN,'wrong OEM bytes')
    need(b[:6]==b'\x7fELF\x01\x01','not ELF32 LE')
    old=headers(b);need([x[0] for x in old]==[6,3,1,1,2],'unexpected phdr layout')
    need(all(x[2]+x[5]<=CODE for x in old if x[0]==1),'native LOAD overlaps')
    for va,w,_ in POINTS:need(struct.unpack_from('<I',b,va-0x100000)[0]==w,'original instruction mismatch')
    with tempfile.TemporaryDirectory() as t:
        t=Path(t);(t/'hook.s').write_text(ASM)
        subprocess.run(['clang','-target','armv7-none-eabi','-march=armv7-a','-marm','-c',str(t/'hook.s'),'-o',str(t/'hook.o')],check=True)
        subprocess.run(['ld.lld','-flavor','gnu','-m','armelf',f'-Ttext={ENTRY:#x}','--entry=mu_started','--no-undefined',str(t/'hook.o'),'-o',str(t/'hook.elf')],check=True)
        syms={}
        for line in subprocess.check_output(['readelf','-sW',str(t/'hook.elf')],text=True).splitlines():
            p=line.split()
            if p and p[-1] in ('mu_started','mu_cleanup'):syms[p[-1]]=int(p[1],16)
        need(len(syms)==2 and syms['mu_started']==ENTRY,'symbol mismatch')
        subprocess.run(['llvm-objcopy','-O','binary','--only-section=.text',str(t/'hook.elf'),str(t/'code.bin')],check=True)
        code=(t/'code.bin').read_bytes()
        need('There are no relocations' in subprocess.check_output(['readelf','-rW',str(t/'hook.elf')],text=True),'relocations')
    n=7;table_len=n*32;off=align(len(b),4096);payload=bytearray(0x100);payload.extend(code)
    need(CODE+len(payload)<STATE,'observer overlaps state')
    stateoff=align(off+len(payload),4096)
    new=[x[:] for x in old];new[0]=[6,off,CODE,CODE,table_len,table_len,4,4]
    new.extend([[1,off,CODE,CODE,len(payload),len(payload),5,4096],[1,stateoff,STATE,STATE,0,4096,6,4096]])
    payload[:table_len]=b''.join(struct.pack('<8I',*x) for x in new)
    result=bytearray(b);struct.pack_into('<I',result,28,off);struct.pack_into('<H',result,44,n)
    allow=set(range(28,32))|set(range(44,46));patches={}
    for va,w,sym in POINTS:
        p=va-0x100000;v=branch(va,syms[sym]);struct.pack_into('<I',result,p,v);allow.update(range(p,p+4));patches[hex(va)]=dict(original=hex(w),branch=hex(v),target=hex(syms[sym]))
    need(all(i in allow for i,(a,c) in enumerate(zip(b,result)) if a!=c),'original bytes unexpectedly changed')
    result.extend(bytes(off-len(result)));result.extend(payload);result.extend(bytes(stateoff-len(result)))
    need(headers(result)==new,'generated PHDR mismatch')
    need(source.read_bytes()==b,'source changed')
    out.mkdir(parents=True,exist_ok=False)
    (out/'btstack.lifecycle-REVIEW-ONLY.bin').write_bytes(result)
    report=dict(original_sha256=PIN,output_sha256=digest(result),original_loads_unchanged=True,phdr_offset=hex(off),observer_state=hex(STATE),patches=patches,installable=False,vehicle_verified=False,qnx_loader_verified=False,observer_only=True,wireless_registration=False)
    (out/'STATUS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stock',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.stock,a.out)
