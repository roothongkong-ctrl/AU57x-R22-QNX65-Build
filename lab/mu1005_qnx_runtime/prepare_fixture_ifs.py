"""Inject the lab-only provider into a private QEMU IFS; no production payload."""
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text()
a='cp "$BIN" "$WORK/$NAME"'
if s.count(a)!=1: raise SystemExit("copy anchor")
s=s.replace(a,a+'\ncp "${QNX_LAB_FIXTURE:?missing lab fixture}" "$WORK/libmu_lab_fixture.so"')
a='\n$NAME\n'
if s.count(a)!=1: raise SystemExit("manifest anchor")
s=s.replace(a,'\n[+raw +page_align perms=0555] libmu_lab_fixture.so\n$NAME\n')
p.write_text(s)
