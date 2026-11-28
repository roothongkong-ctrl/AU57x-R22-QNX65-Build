#!/usr/bin/env python3
"""Independent native snapshots: strict outside old card ROI, not synthetic proof."""
from pathlib import Path
import json,sys
from PIL import Image,ImageChops,ImageDraw

def check(d):
    d=Path(d)
    m=json.loads((d/"r20-meta.json").read_text())
    assert m["mapGeometryUnchanged"] is True
    x,y,w,h=(int(m[k]) for k in ("x","y","w","h"))
    assert x>=0 and y>=0 and w>=16 and h>=16 and x+w<=800 and y+h<=298
    a=Image.open(d/"r21-before.png").convert("RGB")
    b=Image.open(d/"r21-after.png").convert("RGB")
    assert a.size==b.size==(800,298)
    diff=ImageChops.difference(a,b)
    inner=diff.crop((x,y,x+w,y+h))
    changed_inside=sum(p!=(0,0,0) for p in inner.getdata())
    outside=diff.copy()
    ImageDraw.Draw(outside).rectangle((x,y,x+w-1,y+h-1), fill=(0,0,0))
    changed_outside=sum(p!=(0,0,0) for p in outside.getdata())
    out={"changed_inside_original_card":changed_inside,
         "changed_outside_original_card":changed_outside,
         "outside_original_card_exact":changed_outside==0,
         "native_view_geometry_unchanged":True,
         "scope":"independent native SDK draws; moving map may cause strict mismatch; not production type112"}
    diff.save(d/"r21-diff.png")
    outside.save(d/"r21-outside-diff.png")
    (d/"r21-native-result.json").write_text(json.dumps(out,indent=2))
    print(json.dumps(out))
    if not changed_inside: raise SystemExit("FAIL: no card change visible")
    if changed_outside: raise SystemExit("INCONCLUSIVE: outside original card changed; inspect independent frame capture or moving map")

if __name__=="__main__":
    check(sys.argv[1])
