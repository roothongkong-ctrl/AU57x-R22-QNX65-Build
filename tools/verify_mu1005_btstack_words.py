#!/usr/bin/env python3
"""Read-only audit of MU1005 btstack ARM32 instruction words against old R14 references.
Does not install or execute anything on a vehicle. No OEM binaries are committed.
"""
import argparse, hashlib, json, re, struct
from pathlib import Path
from zipfile import ZipFile
REF_HASH="51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
WORDS_PATH="legacy/source/bluetooth/native_binding_words.h"
def main():
 p=argparse.ArgumentParser()
 p.add_argument("--btstack",required=True,type=Path)
 p.add_argument("--legacy-zip",required=True,type=Path)
 p.add_argument("--output",required=True,type=Path)
 a=p.parse_args()
 b=a.btstack.read_bytes()
 sha=hashlib.sha256(b).hexdigest()
 if b[:4]!=b"\x7fELF" or b[4]!=1 or b[5]!=1 or struct.unpack_from("<H",b,18)[0]!=40:
  raise SystemExit("ERROR: expected ELF32 little-endian ARM")
 pos=struct.unpack_from("<I",b,28)[0]
 stride,n=struct.unpack_from("<HH",b,42)
 if stride<32 or pos+stride*n>len(b):raise SystemExit("ERROR: PHDR bounds")
 loads=[]
 for i in range(n):
  kind,offset,va,_,size,_,flags,_=struct.unpack_from("<8I",b,pos+i*stride)
  if kind==1 and flags&1:
   if offset+size>len(b):raise SystemExit("ERROR: PT_LOAD out of range")
   loads.append((va,va+size,offset))
 with ZipFile(a.legacy_zip) as z: source=z.read(WORDS_PATH).decode("utf-8")
 pairs=[(int(a,16),int(v,16)) for a,v in re.findall(r"\{0x([0-9a-fA-F]+)u,0x([0-9a-fA-F]+)u\}",source)]
 rows=[]
 for va,expected in pairs:
  mapped=next((off+va-start for start,end,off in loads if start<=va<=end-4),None)
  actual=struct.unpack_from("<I",b,mapped)[0] if mapped is not None else None
  rows.append({"address":hex(va),"matches":actual==expected,"executable":mapped is not None})
 result={"target":"MHI2_CN_AU57x_K1006_2 / MU1005 R29 70-app.img reference",
  "sha256":sha,"reference_sha_matches":sha==REF_HASH,
  "word_count":len(rows),"matching":sum(r["matches"] for r in rows),
  "static_pass":sha==REF_HASH and len(rows)==40 and all(r["matches"] for r in rows),
  "runtime_owner_proven":False,"real_iap2_endpoint_proven":False,
  "carplay_working_proven":False,"installable":False,"rows":rows}
 a.output.parent.mkdir(parents=True,exist_ok=True)
 a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
 print(f'R14 OEM words {result["matching"]}/{len(rows)}; SHA valid={result["reference_sha_matches"]}; static PASS={result["static_pass"]}')
 return 0 if result["static_pass"] else 1
if __name__=="__main__":raise SystemExit(main())
