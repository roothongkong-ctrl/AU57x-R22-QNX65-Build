#!/usr/bin/env python3
"""Pinned read-only ELF32 ARM audit of MU1005 RF RegisterServerChannel arg3.

The exact OEM image is private and must NOT be included in a public release.
This is structural evidence only, NOT actual native SDK channel creation.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct

OEM_SHA256="51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
CALL_TARGET=0x277a18
SITES={
    0x2464a0:0xe3a02007, # HFP arg3 = immediate 7
    0x2464b0:0xeb00c558, # HFP BL original function
    0x24dca8:0xe3a02004, # SIM arg3 = immediate 4
    0x24dcac:0xeb00a759, # SIM BL same original function
    0x277a18:0xe92d4070, # function entry push r4,r5,r6,lr
    0x277a1c:0xe2504000, # r4 = r0
    0x277a20:0xe1a05001, # r5 = r1, output channel pointer
    0x277a24:0xe1a06002, # r6 = r2, candidate channel-like arg
    0x277a6c:0xe5c4608b, # strb r6,[r4,#0x8b]
    0x277a70:0xe1a01005, # r1 = original caller's r1
    0x277a74:0xe1c468b6, # strh r6,[r4,#0x86]
    0x277a78:0xe1a00004, # r0 = original channel object
    0x277a88:0xebfe2ec6, # forwards to native code
}

def _load_phdrs(buf:bytes):
    if len(buf)<52 or buf[:16][:4]!=b'\x7fELF' or buf[4]!=1 or buf[5]!=1:
        raise ValueError('Not little-endian ELF32')
    if struct.unpack_from('<H',buf,18)[0]!=40 or struct.unpack_from('<H',buf,16)[0]!=2:
        raise ValueError('Not original ARM ET_EXEC')
    off,entsz,count=struct.unpack_from('<I',buf,28)[0],*struct.unpack_from('<HH',buf,42)
    if not 32<=entsz<=128 or count>64 or off+entsz*count>len(buf):
        raise ValueError('Invalid ELF program headers')
    ret=[]
    for i in range(count):
        t,fo,va,_,filesz,memsz,flags,align=struct.unpack_from('<8I',buf,off+i*entsz)
        if t==1:
            if fo+filesz>len(buf) or memsz<filesz:raise ValueError('Invalid PT_LOAD')
            ret.append((va,fo,filesz,flags))
    if not ret:raise ValueError('No PT_LOAD')
    return ret

def word_at(buf:bytes,va:int)->int:
    for base,file_off,size,flags in _load_phdrs(buf):
        if (flags&1) and base<=va and va+4<=base+size:
            return struct.unpack_from('<I',buf,file_off+(va-base))[0]
    raise ValueError(f'ARM instruction not in executable PT_LOAD: {va:#x}')

def bl_target(va:int,insn:int)->int:
    if (insn>>24 & 0xF)!=0xB:
        raise ValueError('Not an ARM unconditional BL')
    off=insn&0xFFFFFF
    if off&0x800000:off-=0x1000000
    return (va+8+4*off)&0xFFFFFFFF

def audit(buf:bytes,check_sha:bool=True)->dict:
    digest=hashlib.sha256(buf).hexdigest()
    if check_sha and digest!=OEM_SHA256:
        raise ValueError('Private MU1005 btstack SHA256 mismatch: refusing cross-firmware offsets')
    for va,insn in SITES.items():
        actual=word_at(buf,va)
        if actual!=insn:
            raise ValueError(f'Original ARM instruction differs at {va:#x}: {actual:#x}')
    for va in (0x2464b0,0x24dcac):
        if bl_target(va,word_at(buf,va))!=CALL_TARGET:
            raise ValueError('Original RF registration call no longer targets expected body')
    return {
        'baseline':'MHI2_CN_AU57x_K1006_2/MU1005',
        'oem_elf':'btstack',
        'sha256':digest,
        'elf32_arm_et_exec_pinned':True,
        'instruction_sites_checked':len(SITES),
        'original_rf_function_va':f'{CALL_TARGET:#x}',
        'original_callers':[
            {'service':'HFP','caller_va':'0x2464b0','arg3_immediate':7},
            {'service':'SIM','caller_va':'0x24dcac','arg3_immediate':4}],
        'arg3_native_dataflow':[
            'caller r2 -> function r6',
            'strb r6, [r4,#0x8b]',
            'strh r6, [r4,#0x86]',
            'original r1 forwarded to internal function'],
        'interpretation':'ARG3_IS_CHANNEL_LIKE (not conclusively proved SCN)',
        'rfcomm_service_channel_range':[1,30],
        'donor_code_rejected_valid_scns_7_to_30':True,
        'iap2_scn_discovered':False,
        'zero_means_auto_alloc_proven':False,
        'native_owner_thread_proven':False,
        'in_car_rf_registration_proven':False,
        'installable':False,
        'vehicle_verified':False}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--btstack',required=True,type=Path,help='Private original ELF; never upload')
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    result=audit(args.btstack.read_bytes())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'PASS':True,'report':str(args.output),'ABI_PROVEN':False,'safe_to_flash':False}))
if __name__=='__main__':main()
