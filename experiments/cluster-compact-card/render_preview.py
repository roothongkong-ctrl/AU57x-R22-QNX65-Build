#!/usr/bin/env python3
from pathlib import Path
from PIL import Image,ImageDraw
import json
W,H=800,298
p=Path('mock')
bg=Image.open(p/'map_only.png').convert('RGBA')
images=[];result={}
for pct in (100,70):
    data=(p/('after%d.rgba'%pct)).read_bytes()
    assert len(data)==W*H*4
    im=Image.frombytes('RGBA',(W,H),data)
    im.save(p/('card_%d.png'%pct));images.append(im)
    cw=(338*pct+50)//100;ch=(126*pct+50)//100
    bad=0; pix=im.load(); baseline=bg.load()
    for y in range(H):
        for x in range(W):
            if x<16 or x>=16+cw or y<18 or y>=18+ch:
                bad+=(pix[x,y]!=baseline[x,y])
    assert bad==0
    result[str(pct)]={'card_width':cw,'card_height':ch,'changed_outside_small_card':bad}
sheet=Image.new('RGB',(1650,405),'#151d24')
d=ImageDraw.Draw(sheet)
d.text((20,14),'Original card (100%)',fill='white')
d.text((843,14),'Original card resized (70%)',fill='#a0ffd7')
sheet.paste(images[0].convert('RGB'),(20,40))
sheet.paste(images[1].convert('RGB'),(840,40))
d.text((20,352),'SYNTHETIC: requires two independent live sources, NOT available yet',fill='#ffc787')
sheet.save(p/'compact_card_before_after.png')
(p/'test_report.json').write_text(json.dumps(result,indent=2)+'\n')
print(result)
