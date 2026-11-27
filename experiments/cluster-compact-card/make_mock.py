#!/usr/bin/env python3
# Synthetic two-input picture; NOT a real CarPlay map or live double stream.
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
W,H=800,298
out=Path('mock');out.mkdir(exist_ok=True)
bg=Image.new('RGBA',(W,H));p=bg.load()
for y in range(H):
  for x in range(W):
    t=min(1.,y/180)
    p[x,y]=(int(205-95*t),int(220-94*t),int(227-96*t),255)
d=ImageDraw.Draw(bg)
d.polygon([(0,172),(310,104),(445,132),(800,98),(800,298),(0,298)],fill='#788793')
d.polygon([(0,212),(445,130),(485,143),(800,160),(800,298),(0,298)],fill='#40515e')
for a,b in [((0,276),(443,136)),((125,298),(460,141)),((333,298),(475,145)),((690,298),(489,148))]:
  d.line([a,b],fill='#e2e5e5',width=4)
d.line([(464,300),(476,240),(522,215),(549,178),(621,143),(775,113)],fill='#235f59',width=19)
d.line([(464,300),(476,240),(522,215),(549,178),(621,143),(775,113)],fill='#70e2b3',width=8)
d.ellipse((452,233,493,273),fill='#e5ecee',outline='#273b4a',width=6)
d.rounded_rectangle((555,225,704,259),radius=8,fill='#edf1f0')
d.text((568,233),'ROAD NAME',fill='#24343b')
card=Image.new('RGBA',(W,H),(0,0,0,0))
c=ImageDraw.Draw(card)
c.rounded_rectangle((16,18,353,143),radius=25,fill=(247,249,251,247),outline=(198,208,216,255),width=2)
c.line([(52,72),(52,49),(101,49)],fill='#27313b',width=10)
c.polygon([(101,34),(118,49),(101,64)],fill='#27313b')
c.text((151,39),'5.1 km',fill='#27313b')
c.text((43,105),'TURN RIGHT',fill='#3d4952')
bg.save(out/'map_only.png')
card.save(out/'original_card_layer.png')
(out/'map.rgba').write_bytes(bg.tobytes())
(out/'card.rgba').write_bytes(card.tobytes())
