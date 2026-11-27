#!/usr/bin/env python3
"""Apply to exactly one original v1.2.14 source file; exit on drift."""
from pathlib import Path
import sys
p=Path(sys.argv[1]) if len(sys.argv)>1 else Path("player/opengl_gpu.cc")
s=p.read_text()
a='#include <stddef.h>\n'
b='#include <stddef.h>\n#include "compact_card_r9.h"\n'
c='            uint8_t* pY = dispBuf->pixels;\n            uint8_t* pU = pY + (w * h);'
d='''            uint8_t* pY = dispBuf->pixels;
            /* R9: only the white card is resized. Requires a CURRENT
             * independently sourced map; else preserve the original picture.
             * Buffered CarPlay stream only; Android Auto unchanged.
             */
            if (g_bufferedReader && w == 800 && h == 298) {
                const uint8_t* cardFrame = compact_card_r9::try_frame(
                    pY, w, h, now_us(), getenv("CLUSTERMAP_CARD_RECT"),
                    compact_card_r9::map_path);
                if (cardFrame) pY = const_cast<uint8_t*>(cardFrame);
            }
            uint8_t* pU = pY + (w * h);'''
for old in (a,c):
    if s.count(old)!=1: raise SystemExit("R9 FAIL CLOSED: unsupported source or code drift")
if 'compact_card_r9.h' in s: raise SystemExit("R9 already applied")
p.write_text(s.replace(a,b,1).replace(c,d,1))
print("PASS: single player source patched; no installed code touched")
