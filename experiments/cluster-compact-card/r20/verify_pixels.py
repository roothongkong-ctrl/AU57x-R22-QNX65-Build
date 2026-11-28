#!/usr/bin/env python3
"""Compare two compositions of ONE SDK capture, never two moving map frames."""
import json
import sys
from pathlib import Path
from PIL import Image, ImageChops

def check(directory):
    d=Path(directory)
    m=json.loads((d/"r20-meta.json").read_text())
    assert m["viewportPixels"]==[800,298] and m["mapGeometryUnchanged"] is True
    assert m["percent"]==70
    x,y,w,h=(int(m[k]) for k in ("x","y","w","h"))
    assert x>=0 and y>=0 and w>=16 and h>=16 and x+w<=800 and y+h<=298
    base=Image.open(d/"r20-base.png").convert("RGBA")
    card=Image.open(d/"r20-card.png").convert("RGBA")
    assert base.size==(800,298) and card.size==(w,h)
    # Reject blank or uniform captures. This is NOT proof the GL map is present.
    assert len(base.convert("RGB").getcolors(32) or range(33))>32
    assert card.getchannel("A").getbbox()
    nw,nh=round(w*.7),round(h*.7)
    small=card.resize((nw,nh),Image.Resampling.LANCZOS)
    original=base.copy()
    original.alpha_composite(card,(x,y))
    compact=base.copy()
    compact.alpha_composite(small,(x,y))
    diff=ImageChops.difference(base.convert("RGB"),compact.convert("RGB"))
    bbox=diff.getbbox()
    assert bbox and bbox[0]>=x and bbox[1]>=y and bbox[2]<=x+nw and bbox[3]<=y+nh
    assert ImageChops.difference(original.convert("RGB"),compact.convert("RGB")).getbbox()
    original.save(d/"r20-original-800x298.png")
    compact.save(d/"r20-compact-800x298.png")
    result={"original_card":[w,h],"compact_card":[nw,nh],"changed_bbox":bbox,
            "map_outside_scaled_card_identical":True,
            "scope":"single captured SDK background plus isolated native card; not live production CarPlay"}
    (d/"r20-pixel-result.json").write_text(json.dumps(result,indent=2))
    print(json.dumps(result))

if __name__=="__main__":
    check(sys.argv[1])
