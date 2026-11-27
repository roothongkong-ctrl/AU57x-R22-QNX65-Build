#!/usr/bin/env python3
"""Classify explicitly labelled private dlerror messages; never emit raw text."""
import json,re,sys
from pathlib import Path
LABELS=("DRIVER_NOW","USB_NOW","DRIVER_LAZY")
DETAIL=re.compile(r"^PRIVATE_LINKER_DETAIL (DRIVER_NOW|USB_NOW|DRIVER_LAZY) (.*)$")
RESULT=re.compile(r"^IAP2_LOADER_(DRIVER_NOW|USB_NOW|DRIVER_LAZY)_(OK|BLOCKED)$")
LIB=re.compile(r"(?<![A-Za-z0-9_.+-])(?:lib[A-Za-z0-9_+-]{1,80}\.so(?:\.[0-9]{1,8}){0,4})(?![A-Za-z0-9_.+-])")
SYM=re.compile(r"(?:unresolved symbol|undefined symbol|symbol not found)\s*[:=]?\s*['\"]?([A-Za-z_][A-Za-z0-9_]{0,200})",re.I)
def classify(lines):
    out={k:{"result":"not_observed","classes":[],"libraries":[],"symbols":[]} for k in LABELS}
    for line in lines:
        line=line.strip()
        m=RESULT.fullmatch(line)
        if m:
            out[m[1]]["result"]=m[2].lower()
            continue
        m=DETAIL.fullmatch(line)
        if not m:continue
        row=out[m[1]];msg=m[2];lower=msg.lower()
        classes=set(row["classes"])
        if any(s in lower for s in ("unresolved symbol","undefined symbol","symbol not found")):
            classes.add("symbol_resolution")
            row["symbols"]=sorted(set(row["symbols"])|set(SYM.findall(msg)))
        if any(s in lower for s in ("cannot be found","cannot load","could not load","unable to load","no such file","failed to load","library not found")):
            classes.add("library_or_path_lookup")
            row["libraries"]=sorted(set(row["libraries"])|set(LIB.findall(msg)))
        if "permission denied" in lower:classes.add("permission")
        if any(s in lower for s in ("invalid elf","wrong elf","bad elf","relocation","unsupported")):classes.add("elf_or_relocation")
        if not classes:classes.add("no_detail" if msg=="(no dlerror)" else "unclassified")
        row["classes"]=sorted(classes)
    return out
def selftest():
    x=classify(["IAP2_LOADER_DRIVER_NOW_BLOCKED",
        "PRIVATE_LINKER_DETAIL DRIVER_NOW Unresolved symbol: example_init",
        "IAP2_LOADER_USB_NOW_OK","IAP2_LOADER_DRIVER_LAZY_BLOCKED",
        "PRIVATE_LINKER_DETAIL DRIVER_LAZY Library libexample.so.1 cannot be found",
        "PRIVATE_LINKER_DETAIL UNKNOWN secret /path",
        "unlabelled private raw text"])
    assert x["DRIVER_NOW"]["symbols"]==["example_init"]
    assert x["USB_NOW"]["result"]=="ok"
    assert x["DRIVER_LAZY"]["libraries"]==["libexample.so.1"]
    assert "secret" not in json.dumps(x)
    assert classify(["PRIVATE_LINKER_DETAIL DRIVER_NOW opaque diagnostic"])["DRIVER_NOW"]["classes"]==["unclassified"]
    print("IPOD_LINKER_CLASSIFIER_SELFTEST_OK")
if __name__=="__main__":
    if sys.argv[1:]==["--selftest"]:selftest()
    else:
        lines=Path(sys.argv[2]).read_text(errors="replace").splitlines()
        out=classify(lines)
        Path(sys.argv[1]).write_text(json.dumps(out,indent=2)+"\n")
        print("IPOD_LINKER_EVIDENCE",json.dumps(out,sort_keys=True))
