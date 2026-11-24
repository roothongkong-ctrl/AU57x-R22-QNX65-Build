#!/usr/bin/env python3
"""Apply MU1005 candidate address profile to pinned GPLv3 upstream sources.
Source/compile review only; runtime remains disabled by default.
No OEM binary, no install, no MFi changes.
"""
from pathlib import Path
import re, subprocess, sys
PIN="20c94f51c0f0ae88b154c12e4f604643ba23022c"
MAP={
0x6c324:0x667b4,0x302e0:0x2d080,0x6a3cc:0x6485c,
0x7d208:0x775f8,0x6b9a4:0x65e34,0x2ebc4:0x2b964,
0x272b8:0x24058,0x26b60:0x23868,0x26f9c:0x23d40,
0x25278:0x21f80,0x6a1d0:0x64660,0x2a9d8:0x27778,
0x2b154:0x27ef4,0x261c4:0x22ecc,0x23948:0x20650,
0x7dbac:0x77f9c,0x7da2c:0x77e1c,0x69b50:0x63fe0,
0x31764:0x2e504,0x220d0:0x1edd8,0x201e4:0x1ceec,
0x6c094:0x66524,0x20d24:0x1da2c}
def once(s,a,b):
    if s.count(a)!=1: raise ValueError("unexpected upstream anchor: "+a)
    return s.replace(a,b,1)
def port(root):
    if subprocess.check_output(["git","-C",str(root),"rev-parse","HEAD"],text=True).strip()!=PIN:
        raise ValueError("wrong upstream commit")
    if subprocess.check_output(["git","-C",str(root),"status","--porcelain"],text=True).strip():
        raise ValueError("upstream worktree not clean")
    d=root/"hook/wireless"
    fp=d/"stock210_fingerprint.h"
    old=fp.read_text()
    found=[]
    def remap(m):
        x=int(m[2],16); found.append(x)
        return m[1]+hex(MAP[x])+m[3]
    new=re.sub(r"({\s*)(0x[0-9a-fA-F]+)(\s*,\s*{)",remap,old)
    if len(found)!=23 or set(found)!=set(MAP):raise ValueError("fingerprint count/profile mismatch")
    new=once(new,"/* Pinned stock 210.81 ARM instruction bytes; generated from verified ELF. */",
             "/* AU57x MU1005 candidate addresses; 16-byte prefixes match private OEM ELF. Internal function semantics remain under review. */")
    p=d/"stock210_abi.c";s=p.read_text()
    s=once(s,"0x6c324u","0x667b4u")
    s=once(s,"0x302e0u","0x2d080u")
    # Keep all upstream guards. No default activation on a vehicle.
    s=once(s,"static uintptr_t verified_base;",
           """/* MU1005 candidate: explicit offline review build only. */
#ifndef AU57X_MU1005_OFFLINE_REVIEW
#define AU57X_MU1005_OFFLINE_REVIEW 0
#endif
static uintptr_t verified_base;""")
    s=once(s,"    Dl_info info;","    if (!AU57X_MU1005_OFFLINE_REVIEW) return 0;\n    Dl_info info;")
    a=d/"advertisement.c";ad=once(a.read_text(),
        "cp210_callsite(__builtin_return_address(0),0x211cc)",
        "cp210_callsite(__builtin_return_address(0),0x1ded4)")
    # All replacements validated before any write.
    for path,data in [(fp,new),(p,s),(a,ad)]:path.write_text(data)
    print("MU1005: 23 fingerprint entries, 2 receiver entry addresses, 1 TXT callsite mapped; no default activation")
if __name__=="__main__":port(Path(sys.argv[1]))
