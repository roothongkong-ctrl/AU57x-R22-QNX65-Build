#!/usr/bin/env python3
"""Strict integration of R17 into UNMODIFIED upstream v1.2.14 player.

Only player/opengl_gpu.cc text gets patched; copy four headers into player/r17.
- Default build: no R17 compiled.
- Experimental: define CLUSTERMAP_R17_CARD70_LAB AND set
  CLUSTERMAP_R17_CARD70=1 AND explicit lab-only CLEAN_MAP_111/CARD112_VERIFIED
  AND original CarPlay --no-ack
  AND source tcp://127.0.0.1:19820 AND 800x298 window.
- If 112 absent/invalid/stale: existing 111 map sent to GLES unchanged.
- Real instrument/phone still NOT validated; this is a source-only build.
"""
import sys
from pathlib import Path
if len(sys.argv)!=2:
    raise SystemExit("usage: patch_real_player.py <upstream/player/opengl_gpu.cc>")
path=Path(sys.argv[1])
s=path.read_text()
def patch_once(before,after,label):
    global s
    n=s.count(before)
    if n!=1:raise SystemExit(f"FAIL CLOSED {label}: original anchor count={n}")
    s=s.replace(before,after,1)

patch_once(
"""#include <libavutil/cpu.h>
}

// ---------------- Configuration & Performance Knobs ----------------""",
"""#include <libavutil/cpu.h>
}
#ifdef CLUSTERMAP_R17_CARD70_LAB
#include "r17/r17_card_sidecar.h"
static r17::Sidecar g_r17_card;
#endif

// ---------------- Configuration & Performance Knobs ----------------""",
"compile-time sidecar include"
)
patch_once(
"""    g_isSocket = (strncmp(g_videoSource, "tcp://", 6) == 0 || is_unix_sock);
    LOG("stream-player starting (source: %s, loop: %d, pool: %d)",""",
"""    g_isSocket = (strncmp(g_videoSource, "tcp://", 6) == 0 || is_unix_sock);
#ifdef CLUSTERMAP_R17_CARD70_LAB
    {
        const char *opt=getenv("CLUSTERMAP_R17_CARD70");
        const char *mapOnly=getenv("CLUSTERMAP_R17_CLEAN_MAP_111");
        const char *verified=getenv("CLUSTERMAP_R17_CARD112_VERIFIED");
        /* A normal full 111 frame already includes the card. Never assume
         * it is clean from the socket address or window dimensions alone.
         * This explicit lab contract is not an on-car capability probe. */
        /* Not a general overlay feature! Explicitly require the original
         * card-free MIB CarPlay video path and 800x298 real window.
         * No change at all to Android Auto, file playback or any other size.
         */
        if(opt && strcmp(opt,"1")==0 &&
           mapOnly && strcmp(mapOnly,"1")==0 &&
           verified && strcmp(verified,"1")==0 &&
           g_noAck && windowWidth==800 && windowHeight==298 &&
           strcmp(g_videoSource,"tcp://127.0.0.1:19820")==0) {
            if(g_r17_card.start(now_us()))
                LOG("R17 LAB: listening for independent 112 card video");
            else
                LOGE("R17 LAB: sidecar startup failed; stock 111 preserved");
        } else if(opt && strcmp(opt,"1")==0) {
            LOG("R17 LAB refused: requires explicitly verified independent map/card sources; stock 111 unchanged");
        }
    }
#endif
    LOG("stream-player starting (source: %s, loop: %d, pool: %d)",""",
"runtime triple-gated CarPlay initialization"
)
patch_once(
"""                        // Copy V plane
                        for (int y = 0; y < h / 2; ++y) {
                            memcpy(dstV + y * (w / 2), frame->data[2] + y * frame->linesize[2], w / 2);
                        }

                        g_framePool.publishWrite();""",
"""                        // Copy V plane
                        for (int y = 0; y < h / 2; ++y) {
                            memcpy(dstV + y * (w / 2), frame->data[2] + y * frame->linesize[2], w / 2);
                        }

#ifdef CLUSTERMAP_R17_CARD70_LAB
                        /* Exactly one boundary before stock pool publish.
                         * Other pixel locations are identical to 111.
                         * Never holds a GL/UI lock on 112 socket reads. */
                        if(g_r17_card.active())
                            (void)g_r17_card.compose_111(buf->pixels,w,h,now_us());
#endif

                        g_framePool.publishWrite();""",
"real decoded 111 buffer integration"
)
patch_once(
"""    LOG("shutting down: waiting for decoder thread");
    pthread_join(decThread, NULL);

#ifdef __QNX__""",
"""    LOG("shutting down: waiting for decoder thread");
    pthread_join(decThread, NULL);
#ifdef CLUSTERMAP_R17_CARD70_LAB
    g_r17_card.stop(); /* only after decoder stops touching map frames */
#endif

#ifdef __QNX__""",
"safe shutdown with original frame lifetime"
)
path.write_text(s)
print("PASS R17 exact v1.2.14 player patched at THREE code paths;"
      " compile+runtime OFF by default; no 110/111 transport rewrite")
