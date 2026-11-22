#!/usr/bin/env python3
"""Strict, read-only ELF32 ARM OEM delegate audit for exact MU1005 binaries.
No OEM bytes or binaries are written to the report. Not an installer/hook.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

SHA = {
 'dio_manager': 'e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c',
 'libairplay.so': '193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5',
 'smartphone_integrator': '2c61e7a17d5c77df2f97023284b948b66dc4052010acad74be9bfa63b339de0b',
}

class AuditError(ValueError): pass

class ELF:
 def __init__(self, raw):
  self.raw=raw
  if len(raw)<52 or raw[:6]!=b'\x7fELF\x01\x01' or self.u16(18)!=40:
   raise AuditError('expected little-endian ARM ELF32')
  off,nent,nstr,ix=self.u32(32),self.u16(46),self.u16(48),self.u16(50)
  if nent!=40 or not 1<=nstr<=128 or ix>=nstr or off+40*nstr>len(raw):
   raise AuditError('malformed section table')
  self.sh=[struct.unpack_from('<IIIIIIIIII',raw,off+i*40) for i in range(nstr)]
  names=self.take(self.sh[ix][4],self.sh[ix][5]); self.sections={}
  for h in self.sh:
   name=self.cstr(names,h[0]);
   if name: self.sections[name]=dict(addr=h[3],offset=h[4],size=h[5],link=h[6],entsize=h[9])
 def take(self,at,n):
  if at+n>len(self.raw):raise AuditError('truncated ELF data')
  return self.raw[at:at+n]
 @staticmethod
 def cstr(data,idx):
  if idx>=len(data):raise AuditError('string table overflow')
  end=data.find(b'\0',idx)
  if end<0:raise AuditError('unterminated ELF symbol')
  return data[idx:end].decode('utf-8','replace')
 def u16(self,n):return struct.unpack_from('<H',self.raw,n)[0]
 def u32(self,n):return struct.unpack_from('<I',self.raw,n)[0]
 def word(self,va):
  for name,s in self.sections.items():
   if name!='.bss' and s['addr']<=va and va+4<=s['addr']+s['size']:
    return struct.unpack('<I',self.take(s['offset']+va-s['addr'],4))[0]
  raise AuditError(f'no file-backed ARM word at 0x{va:x}')
 def symbols(self):
  s=self.sections['.dynsym']
  if s['entsize']!=16:raise AuditError('unexpected dynsym size')
  h=self.sh[s['link']]; names=self.take(h[4],h[5]); ans={}
  for idx in range(s['size']//16):
   st=struct.unpack_from('<IIIBBH',self.raw,s['offset']+16*idx)
   ans[idx]=dict(name=self.cstr(names,st[0]),address=st[1],size=st[2])
  return ans
 def reloc(self):
  syms=self.symbols();ans={}
  for name in ('.rel.dyn','.rel.plt'):
   sec=self.sections[name]
   if sec['entsize']!=8:raise AuditError('bad ARM relocation format')
   for i in range(sec['size']//8):
    addr,info=struct.unpack_from('<II',self.raw,sec['offset']+8*i)
    sym,t=info>>8,info&255
    if t in (21,22) and sym in syms:ans[addr]={**syms[sym], 'kind':t}
  return ans
 def branch_calls(self,target):
  s=self.sections['.text'];result=[]
  for pc in range(s['addr'],s['addr']+s['size']-3,4):
   w=self.word(pc)
   if w & 0xff000000 != 0xeb000000:continue
   d=(w&0xffffff)<<2
   if d & 0x2000000:d-=0x4000000
   if (pc+8+d)&0xffffffff==target:result.append(pc)
  return result

def must(elf,va,expected):
 if elf.word(va)!=expected:raise AuditError(f'ABI instruction changed at 0x{va:x}')

def audit(binary):
 data={k:Path(p).read_bytes() for k,p in binary.items()}
 hashes={k:hashlib.sha256(v).hexdigest() for k,v in data.items()}
 if hashes!=SHA:raise AuditError('not exact pinned MU1005 OEM ELF set; refuse ABI inference')
 dio=ELF(data['dio_manager']);air=ELF(data['libairplay.so']);si=ELF(data['smartphone_integrator'])
 if not si.sections.get('.dynsym'):raise AuditError('no original SI dynsym')
 dio_sym=dio.symbols();air_sym=air.symbols()
 def unique(syms,name):
  found=[x for x in syms.values() if x['name']==name]
  if len(found)!=1:raise AuditError('missing or duplicate '+name)
  return found[0]
 imp=unique(dio_sym,'AirPlayReceiverServerSetDelegate')
 ap_set=unique(air_sym,'AirPlayReceiverServerSetDelegate')
 ap_session=unique(air_sym,'AirPlayReceiverSessionSetDelegate')
 sess_import=unique(dio_sym,'AirPlayReceiverSessionSetDelegate')
 if dio.branch_calls(sess_import['address'])!=[0x159448]:
  raise AuditError('DIO session delegate setter callsite changed')
 if dio.branch_calls(imp['address'])!=[0x15df68]:
  raise AuditError('original DIO registration callsite not unique')
 # Original library stores 3x16+12 bytes for Server; 3x16+8 for Session.
 for va,w in ((0x1cef4,0xe280c010),(0x1cef8,0xe8b4000f),
              (0x1cf10,0xe8940007),(0x1cf14,0xe88c0007),
              (0x1fb4c,0xe280c014),(0x1fb68,0xe8940003),(0x1fb6c,0xe88c0003)):
  must(air,va,w)
 # DIO builds 60-byte stack delegate at sp+76; r5+8 owner context;
 # passes original AirPlay server object r5+12 in r0 and stack delegate r1.
 for va,w in ((0x15de80,0xe28d804c),(0x15de88,0xe3a0203c),
              (0x15df50,0xe5953008),(0x15df54,0xe1a01008),
              (0x15df58,0xe595000c),(0x15df64,0xe58d304c),
              (0x15db08,0xe08f4004),(0x15daf4,0xe59f4a44)):
  must(dio,va,w)
 r4=(0x15db10+dio.word(0x15e540))&0xffffffff
 if r4!=dio.sections['.got']['addr']:
  raise AuditError('original ARM PIC GOT base mismatch')
 rel=dio.reloc()
 mapping=[(12,0x15deb8,'s_serverCopyPropertyCallback'),
          (20,0x15de94,'s_sessionCreatedCallback'),
          (24,0x15deac,'s_sessionCreationFailedCallback'),
          (28,0x15dea0,'s_sessionScreenCreatedCallback'),
          (32,0x15dec4,'s_audioTypeCallback'),
          (36,0x15ded0,'s_waitAudioAvailableAckEvent'),
          (40,0x15df00,'s_txAudioDumpingCallback'),
          (44,0x15df18,'s_rxAudioDumpingCallback'),
          (48,0x15df30,'s_txAudioFlushCallback'),
          (52,0x15df48,'s_rxAudioFlushCallback'),
          (56,0x15dee8,'s_initAudioDumpingCallback')]
 callbacks=[]
 for offset,pc,shortname in mapping:
  insn=dio.word(pc)
  if insn&0x0ffff000!=0x059f3000:raise AuditError('callback GOT load changed')
  litva=(pc+8+(insn&0xfff))&~3
  got=(r4+dio.word(litva))&0xffffffff
  actual=rel.get(got)
  if actual is None or actual['kind']!=21 or shortname not in actual['name']:
   raise AuditError('GOT symbol mismatch at stack slot +'+str(offset))
  callbacks.append(dict(offset_bytes=offset,symbol=shortname,
                        callback_va=f"0x{actual['address']:x}",got_va=f'0x{got:x}'))
 # The *same* original callback is the sole session registration path.
 # s_sessionCreatedCallback uses a zeroed 56-byte stack delegate, with
 # its DIO callback context at +0 and the actual session in r0.
 for va,w in ((0x1593a4,0xe3a02038),(0x1593a8,0xe1a0000d),
              (0x1593bc,0xe58d6000),(0x159438,0xe1a0100d),
              (0x15943c,0xe1a00005),(0x159444,0xe58d3034)):
  must(dio,va,w)
 session_mapping=[(12,0x1593b4,'s_sessionFinalizedCallback'),
                  (16,0x1593dc,'s_handleSessionControl'),
                  (20,0x1593c4,'s_sessionCopyProperty'),
                  (28,0x1593d0,'s_sessionNotifyProperty'),
                  (32,0x1593e8,'s_sessionModesChangedCallback'),
                  (36,0x1593f4,'s_sessionRequestUICallback'),
                  (40,0x159400,'s_duckAudioCallback'),
                  (44,0x15940c,'s_unduckAudioCallback'),
                  (48,0x159424,'s_audioTornDownCallback'),
                  (52,0x159430,'s_airplayErrorCallback')]
 session_fields=[]
 for offset,pc,shortname in session_mapping:
  insn=dio.word(pc)
  if insn&0x0ffff000!=0x059f3000:
   raise AuditError(f'session callback PIC load changed at 0x{pc:x}')
  got=(r4+dio.word((pc+8+(insn&0xfff))&~3))&0xffffffff
  v=rel.get(got)
  if not v or v['kind']!=21 or shortname not in v['name']:
   raise AuditError(f'session callback GOT mismatch at +{offset}')
  session_fields.append(dict(offset_bytes=offset,symbol=shortname,
                              callback_va=f"0x{v['address']:x}",got_va=f'0x{got:x}'))
 return dict(valid=True,source='exact MU1005 OEM ELF32',research_only=True,
             safe_to_flash=False,vehicle_verified=False,oem_sha256=hashes,
             DIO_delegate=dict(import_name=imp['name'],callsite_va='0x15df68',
                  registration_server='[DIO local r5 + 12]',
                  stack_delegate='sp + 76',delegate_size=60,
                  context='[DIO local r5 + 8] stored at +0',
                  airplay_copy_dst_offset=16,airplay_server_setter=f"0x{ap_set['address']:x}",
                  callbacks=callbacks),
             AirPlay_session_delegate=dict(setter_va=f"0x{ap_session['address']:x}",
                                          DIO_callsite='0x159448',
                                          delegate_context='DIO sessionCreatedCallback r2 stored at +0',
                                          session_ref='DIO sessionCreatedCallback r1, passed in r0',
                                          copied_bytes=56,dst_offset=20,
                                          callbacks=session_fields),
             not_proven=['production interposition/owner attachment','real MFi/Apple pairing',
                         'live DIO/SI and physical cable callbacks',
                         'real audio/video/control', 'safe SD installation'])

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--dio',required=True);p.add_argument('--airplay',required=True)
 p.add_argument('--si',required=True);p.add_argument('--out');a=p.parse_args()
 try:r=audit({'dio_manager':a.dio,'libairplay.so':a.airplay,'smartphone_integrator':a.si})
 except (AuditError,KeyError,OSError,struct.error) as e:
  print(json.dumps({'valid':False,'safe_to_flash':False,'error':str(e)},ensure_ascii=False));return 2
 out=json.dumps(r,indent=2,ensure_ascii=False,sort_keys=True)+'\n'
 if a.out:Path(a.out).write_text(out,encoding='utf-8')
 else:print(out)
 return 0
if __name__=='__main__':raise SystemExit(main())
