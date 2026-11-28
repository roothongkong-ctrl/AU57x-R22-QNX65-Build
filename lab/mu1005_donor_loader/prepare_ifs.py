"""Run the unmodified pinned ARM donor with --check-config in isolated IFS."""
from pathlib import Path
import sys
p=Path(sys.argv[1])
s=p.read_text()
def replace(a,b):
    global s
    if s.count(a)!=1:raise ValueError("unexpected harness anchor: "+a)
    s=s.replace(a,b)
replace('    $NAME\n    display_msg "@@QNXRUN-END@@"','    $NAME --check-config\n    display_msg "@@QNXRUN-END@@"')
replace('devc-serdebug\n$NAME\nEOF','devc-serdebug\n[+raw +page_align perms=0555] $NAME\nEOF')
replace("if ! grep -q '@@QNXRUN-START@@' \"$LOG\"; then",'[ -z "${QEMU_KEEP_LOG:-}" ] || cp "$LOG" "$QEMU_KEEP_LOG"\n'+"if ! grep -q '@@QNXRUN-START@@' \"$LOG\"; then")
p.write_text(s)
