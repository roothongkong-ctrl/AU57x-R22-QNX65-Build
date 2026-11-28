#!/usr/bin/env python3
"""Optional offline renderer, NOT the original AMap card and NOT car-installable.
Consumes one COMPLETE LuKa EVT_RGD_UPDATE CPHB frame, never connects to the car.
Source contract: luka-dev/mib2q-carplay-rgi hook/routeguidance/rgd_hook.c,
hook/framework/bus_protocol.h and THIRD_PARTY_NAV_APPS.md (2026-11-28).
No map pixels outside the small card rectangle change. No guessed maneuvers.
"""
import struct
from PIL import Image, ImageDraw, ImageFont
def model(frame):
    if len(frame)<16 or len(frame)>131088:return None
    magic,seq,event,flags,reserved,n=struct.unpack(">IIHBBI",frame[:16])
    if magic!=0x43504842 or event!=0x20 or reserved or flags&~1 or n!=len(frame)-16:return None
    try:
        values={}
        for line in frame[16:].decode("utf-8","strict").splitlines():
            key,kind,value=line.split(":",2)
            if len(kind)!=1 or key in values:return None
            values[key]=value
        if values.get("source_name") not in ("AMap","高德地图","高德"):return None
        if values.get("source_supports_rg")!="1" or values.get("route_state")!="1":return None
        if not 0<int(values["route_generation"])<=0xffffffff:return None
        if values.get("disconnect_reason"):return None
        slots=values["maneuver_list"].split(",")
        if len(slots)>16 or any(not s.isascii() or not s.isdecimal() or int(s)>31 for s in slots):return None
        if len(set(slots))!=len(slots):return None
        k="m"+str(int(slots[0]))+"_"
        if values[k+"full"]!="1" or not 1<=int(values[k+"ver"])<=65535:return None
        # Deliberately limited to unambiguous basic turns. Never approximate
        # roundabouts, exits, or lane semantics with an unrelated arrow.
        direction={"1":"left","2":"right","3":"straight"}.get(values[k+"type"])
        if not direction:return None
        distance=int(values["dist_maneuver_m"])
        if not 0<=distance<=100000:return None
        return direction,distance
    except (ValueError,KeyError,UnicodeError,OverflowError):return None

def compose(clean, frame, original_rect, *, map_only=False, age_ms=None):
    """Only trusted current map-only frame + fresh coherent RGI snapshot.
    Caller supplies real original rectangle; NO production geometry inferred."""
    if not map_only or age_ms is None or not 0<=age_ms<=2000:return clean.copy()
    m=model(frame)
    if not m or clean.size!=(800,298) or clean.mode!="RGB":return clean.copy()
    x,y,w,h=original_rect
    if min(x,y)<0 or w<140 or h<80 or x+w>800 or y+h>298:return clean.copy()
    sw,sh=round(w*.7),round(h*.7)
    card=Image.new("RGB",(w,h),"white")
    d=ImageDraw.Draw(card)
    direction,dist=m
    # Use explicit unambiguous arrows; no fabricated road name or ETA.
    coords={"left":[(.38,.18),(.13,.4),(.38,.62),(.38,.47),(.58,.47),(.58,.86),(.73,.86),(.73,.32),(.38,.32)],
            "right":[(.62,.18),(.87,.4),(.62,.62),(.62,.47),(.42,.47),(.42,.86),(.27,.86),(.27,.32),(.62,.32)],
            "straight":[(.5,.12),(.19,.45),(.39,.45),(.39,.86),(.61,.86),(.61,.45),(.81,.45)]}
    d.polygon([(round(a*h*.85),round(b*h)) for a,b in coords[direction]],fill="black")
    label=f"{dist} m" if dist<1000 else f"{dist/1000:.1f} km"
    font=ImageFont.load_default(size=max(12,min(h//3,w//9)))
    if d.textlength(label,font=font)>w-h:return clean.copy()
    d.text((h, h//2),label,font=font,fill="black",anchor="lm")
    result=clean.copy()
    result.paste(card.resize((sw,sh),Image.Resampling.LANCZOS),(x,y))
    return result

def selftest():
    from PIL import ImageChops
    def wire(**updates):
        v={"source_name":"AMap","source_supports_rg":"1","route_state":"1",
           "route_generation":"3","maneuver_list":"4","m4_full":"1","m4_ver":"2",
           "m4_type":"2","dist_maneuver_m":"450"}
        v.update(updates)
        payload="".join(f"{k}:s:{v}\n" for k,v in v.items()).encode()
        return struct.pack(">IIHBBI",0x43504842,1,0x20,1,0,len(payload))+payload
    base=Image.new("RGB",(800,298),(12,90,128))
    rect=(170,12,350,118) # synthetic test only
    def render(p,**kw):return compose(base,p,rect,map_only=kw.get("map_only",True),age_ms=kw.get("age_ms",10))
    bbox=ImageChops.difference(base,render(wire())).getbbox()
    assert bbox==(170,12,415,95),bbox
    for bad in [wire(route_state="0"),wire(source_supports_rg="0"),wire(m4_type="28"),wire(dist_maneuver_m="-1"),wire(maneuver_list=""),wire()[:-1],wire(source_name="Other")]:
        assert ImageChops.difference(base,render(bad)).getbbox() is None
    for opts in [{"map_only":False},{"age_ms":2001},{"age_ms":None}]:
        assert ImageChops.difference(base,render(wire(),**opts)).getbbox() is None
    print("PASS: synthetic RGI framing, supported turns, 70% overlay bounds, stale/invalid/unsupported input preserves original map")
if __name__=="__main__":selftest()
