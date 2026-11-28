#!/usr/bin/env python3
"""Add the same card compositor at the NvMedia memory decoder boundary.
NvSS direct output remains stock. All behavior is opt-in; no firmware write.
"""
from pathlib import Path
import subprocess, sys
if len(sys.argv)!=2: raise SystemExit("usage: prepare_player.py <upstream clone>")
repo=Path(sys.argv[1]).resolve()
here=Path(__file__).resolve().parents[1]
p=repo/"player/opengl_gpu.cc"
subprocess.run([sys.executable,str(here/"r17/patch_real_player.py"),str(p)],check=True)
s=p.read_text()
anchor="""    for (int y = 0; y < h / 2; ++y) memcpy(dstV + y * (w / 2), srcV + (size_t)y * pitchV, w / 2);
    g_framePool.publishWrite();"""
if s.count(anchor)!=1: raise SystemExit("NvMedia memory boundary mismatch")
s=s.replace(anchor,anchor.replace("    g_framePool.publishWrite();","""#ifdef CLUSTERMAP_R17_CARD70_LAB
    if(g_r17_card.active())
        (void)g_r17_card.compose_111(buf->pixels,w,h,now_us());
#endif
    g_framePool.publishWrite();"""),1)
anchor="""        ack_connect();
        pthread_t hwThread;"""
if s.count(anchor)!=1: raise SystemExit("NvSS direct boundary mismatch")
s=s.replace(anchor,"""#ifdef CLUSTERMAP_R17_CARD70_LAB
        if(g_r17_card.active()) {
            g_r17_card.stop();
            LOG("R24 LAB: direct NvSS output has no I420 buffer; preserving unmodified map");
        }
#endif
"""+anchor,1)
p.write_text(s)
import shutil
for rel in ["r9/compact_card_r9.h","r12/dual_stream_joiner.h","r14/card112_ffmpeg_decoder.h","r17/r17_card_sidecar.h"]:
    dest=repo/"player/r17"/(rel if not rel.startswith("r17/") else Path(rel).name)
    dest.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(here/rel,dest)
subprocess.run(["git","-C",str(repo),"diff","--check"],check=True)
assert s.count("g_r17_card.compose_111(buf->pixels,w,h,now_us())")==2
print("R24: SW + NvMedia memory integration; NvSS direct preserves original map. Real type112 and ROI remain unverified.")
