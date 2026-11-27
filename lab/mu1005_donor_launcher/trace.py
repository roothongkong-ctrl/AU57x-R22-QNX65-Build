#!/usr/bin/env python3
"""R105.157: trace the stock launcher used by the successful donor architecture.
Read only; never loads a donor binary, changes config, or authorizes installation.
"""
import hashlib,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"mu1005_first_link_matrix"))
from probe import Binary

TARGET="08896149dcdcc2bc5a6b22ce78682ad5949fbeb8fdc0fb1249162861e40914b8"
TARGET_BT="51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"
DONORS={
 "MU1102":"896c9d58ede2457f0157273ddd56ff113701fc7890df7c8418b62ced1ba9daf7",
 "MU1367":"dd80ce8f6d95589cc4db6674150a595b8412d132e2d9e689fbfe9f72e5789251",
 "MU1440":"d2b55046b8f22302c27811ab2070dc87f0408702c8764b43ad2ff135a66444a0",
}
def exact(root,name,pin):
 for p in sorted(root.rglob(name)):
  if p.is_file() and p.resolve().is_relative_to(root):
   d=p.read_bytes()
   if hashlib.sha256(d).hexdigest()==pin:return d
 raise ValueError("missing exact target "+name)
def main(root,out):
 exact(root,"btstack",TARGET_BT)
 b=Binary(exact(root,"connectivity_launcher",TARGET))
 # R94 already established the presence of these references; here recover
 # direct callsites/owners and candidate config path references in the launcher,
 # NOT another scan of Bluetooth enableIap or donor fixed addresses.
 setters=b.symbol("setExecutable")
 readers=b.symbol("JSONReader")
 paths=[b.marker(s,True) for s in ("connectivity.json","/etc/eso/production/connectivity.json","/mnt/system/etc/eso/production/connectivity.json")]
 result={"scope":"STATIC_LAUNCHER_CALLSITE_CANDIDATES_ONLY","target_launcher_sha256":TARGET,
         "target_btstack_sha256":TARGET_BT,"donor_hash_matches":{k:v==TARGET_BT for k,v in DONORS.items()},
         "setExecutable":setters,"JSONReader":readers,"config_path_candidates":paths,
         "runtime_reader_verified":False,"config_effective_verified":False,"installable":False}
 out.write_text(json.dumps(result,indent=2)+"\n")
 print(json.dumps(result,sort_keys=True))
 print("R105157_LAUNCHER_CANDIDATES_ONLY_NO_INSTALL")
 if not setters["direct_call_count"] and not readers["direct_call_count"] and not any(p["code_reference_count"] for p in paths):
  raise SystemExit(3)
if __name__=="__main__":
 if sys.argv[1:]==["--selftest"]:
  assert len(set(DONORS.values()))==3 and TARGET_BT not in DONORS.values()
  print("R105157_DONOR_HASH_GATE_SELFTEST_OK")
 else:
  main(Path(sys.argv[1]).resolve(strict=True),Path(sys.argv[2]))
