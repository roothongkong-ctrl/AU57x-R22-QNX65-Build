#!/usr/bin/env python3
"""Offline-only: compare ARM ELF against exact AU57x MU1005 firmware ABI.
No QNX dynamic execution, no SD installation, no vehicle actions.
"""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

EXPECTED={
 'dio_manager':'e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c',
 'libairplay.so':'193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5',
 'btstack':'51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566',
 'smartphone_integrator':'2c61e7a17d5c77df2f97023284b948b66dc4052010acad74be9bfa63b339de0b',
 'ipod-drvr-iap2.so':'dda3d733bb62539fa2949482512464a524db8e8fe74764890d6b677975cb72c9',
}
AIRPLAY_EXPORTS=[
 'CFGetTypeID','CFStringGetTypeID','AirPlayReceiverServerGetTypeID',
 'CFStringCreateWithCString','CFStringGetCString','CFRelease',
 'AirPlayReceiverServerSetProperty','AirPlayReceiverServerControl',
 'gAirPlayReceiverServer','AirPlayReceiverServerGetDispatchQueue']
DIO_SYMBOL_HINTS=['CBluetoothControllerJob_GetBTInterface',
                  'CBluetoothControllerEvent_HeadUnitBTInterfaceAvailable',
                  'CAirPlayServiceEvent_StartAirPlay']

def sha(p):
 h=hashlib.sha256()
 with open(p,'rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
 return h.hexdigest()

def readelf(p,*args):
 cp=subprocess.run(['readelf',*args,str(p)],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=20)
 if cp.returncode:raise RuntimeError('readelf '+repr(args)+': '+cp.stderr[:400])
 return cp.stdout

def inspect(p):
 hdr=readelf(p,'-h');dyn=readelf(p,'-d');rel=readelf(p,'-r');attrs=readelf(p,'-A')
 def get(regex,source=hdr):
  m=re.search(regex,source,re.M);return m.group(1).strip() if m else None
 flag=get(r'^\s*Flags:\s*(0x[0-9a-fA-F]+)')
 deps=re.findall(r'\(NEEDED\).*?Shared library: \[([^\]]+)\]',dyn)
 symbol_strings=readelf(p,'--dyn-syms','--wide')
 missing=[]
 return {
  'sha256':sha(p), 'class':get(r'^\s*Class:\s*(.+)$'),
  'data':get(r'^\s*Data:\s*(.+)$'),
  'machine':get(r'^\s*Machine:\s*(.+)$'),
  'type':get(r'^\s*Type:\s*(.+)$'),
  'e_flags':flag,
  'e_flags_match_exact_oem':flag=='0x5000002',
  'needed':deps,
  'qnx_libc_needed_directly':'libc.so.3' in deps,
  'has_qnx_loader':'/usr/lib/ldqnx.so.2' in readelf(p,'-l') if 'EXEC' in (get(r'^\s*Type:\s*(.+)$') or '') else None,
  'vfpv3_d16':'Tag_FP_arch: VFPv3-D16' in attrs,
  'unaligned_access_tag':get(r'^\s*Tag_CPU_unaligned_access:\s*(.+)$',attrs),
  'rel32_dynamic':sum('R_ARM_REL32' in x for x in rel.splitlines()),
  'undefined_symbols':sorted(set(re.findall(r'\sUND\s+(\S+)\s*$',symbol_strings,re.M))),
  'dynamic_symbols':symbol_strings,
 }

def evaluate(candidate):
 p=Path(candidate)
 x=inspect(p)
 checks={
  'ELF32':x['class']=='ELF32',
  'ARM':x['machine']=='ARM',
  'EABI5':x['e_flags'] is not None and (int(x['e_flags'],16)&0xff000000)==0x05000000,
  'QNX_MU1005_FLAGS_EXACT':x['e_flags_match_exact_oem'],
  'DIRECT_QNX_LIBC_NEEDED':x['qnx_libc_needed_directly'],
  'NO_ARM_REL32_DYNAMIC':x['rel32_dynamic']==0,
  'NO_UNRESOLVED_AEABI_MEMCPY':not any(s.startswith('__aeabi_memcpy') for s in x['undefined_symbols']),
 }
 if not checks['ELF32'] or not checks['ARM'] or not checks['EABI5']:
  checks['DIRECT_QNX_LIBC_NEEDED']=False
 return x,checks

def main():
 ap=argparse.ArgumentParser()
 ap.add_argument('--candidate',required=True)
 ap.add_argument('--oem-dir',help='optional directory containing your OWN five verified MU1005 ELF files')
 ap.add_argument('--output',help='write JSON report')
 a=ap.parse_args()
 x,checks=evaluate(a.candidate)
 out={'candidate':str(a.candidate),
      'readiness_checks':checks,
      'reference':{'firmware':'MHI2_CN_AU57x_K1006_2 MU1005', 'expected_qnx_flags':'0x5000002'},
      'candidate_attributes':{k:v for k,v in x.items() if k!='dynamic_symbols'}}
 if a.oem_dir:
  d=Path(a.oem_dir)
  audit={}
  for n,digest in EXPECTED.items():
   p=d/n; audit[n]={'present':p.is_file(),'expected_sha256':digest}
   if p.is_file():audit[n]['actual_sha256']=sha(p);audit[n]['sha256_match']=audit[n]['actual_sha256']==digest
  air=d/'libairplay.so';dio=d/'dio_manager';
  if air.is_file() and audit['libairplay.so'].get('sha256_match'):
   syms=inspect(air)['dynamic_symbols']
   audit['airplay_exports']={s:bool(re.search(r'\s'+re.escape(s)+r'\s*$',syms,re.M)) for s in AIRPLAY_EXPORTS}
  if dio.is_file() and audit['dio_manager'].get('sha256_match'):
   b=dio.read_bytes()
   audit['dio_owner_static_markers']={s:(s.encode() in b) for s in DIO_SYMBOL_HINTS}
   audit['dio_names']={s:(s.encode()+b'\x00' in b) for s in ['carplay0','uap0','startServer','stopServer','interfaceName']}
  out['oem_audit']=audit
 out['verdict']='OFFLINE_ONLY_READY_FOR_QNX_NATIVE_REVIEW' if all(checks.values()) else 'BLOCKED_NOT_NATIVE_QNX'
 out['runtime_verified']=False
 out['wireless_carplay_verified']=False
 out['installable']=False
 data=json.dumps(out,indent=2,ensure_ascii=False)+'\n'
 if a.output:Path(a.output).write_text(data,encoding='utf8')
 print(data)
 return 0 if all(checks.values()) else 20
if __name__=='__main__':sys.exit(main())
