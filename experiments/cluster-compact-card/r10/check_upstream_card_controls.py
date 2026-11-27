#!/usr/bin/env python3
"""Inspect REAL MIB Cluster Map v1.2.14 sources for a card-only scale control.

A negative finding is limited to these public sources; it makes no claims
about undocumented private iOS APIs or the user's real phone.
This program NEVER modifies MIB files or CarPlay parameters.
"""
import argparse
import json
import re
from pathlib import Path

def require(text, pattern, label):
    if not re.search(pattern, text, re.M):
        raise AssertionError(f"Upstream source changed / missing {label}: {pattern}")

def audit(root):
    root = Path(root)
    config = (root / "config/gal_dualscreen.conf").read_text()
    hook = (root / "thirdparty/mhi2_altscreen_carplay/libaltscreen111_gen2.c").read_text()
    player = (root / "player/opengl_gpu.cc").read_text()
    manual = (root / "docs/CarPlay.md").read_text()

    # These are REAL existing knobs, checked against the exact upstream source.
    for name in ("CLUSTERMAP_CARPLAY_SCALE",
                 "CLUSTERMAP_CARPLAY_FRAME_SCALE",
                 "CLUSTERMAP_CARPLAY_INSETS",
                 "CLUSTERMAP_CARPLAY_VIEW",
                 "CLUSTERMAP_CARPLAY_CARD_POSITION"):
        require(config, r"\b" + name + r"\b", name)
    require(config, r"CLUSTERMAP_CARPLAY_SCALE=<percent", "global SCALE semantics")
    require(config, r"FRAME_SCALE.*percent", "global FRAME_SCALE semantics")
    require(config, r"VIEW:\s+full\s*=\s*map with the instruction card", "full/combined display")
    require(config, r"CARD_POSITION:\s+left, right or top", "only card position")
    require(manual, r"text, icons and card get smaller", "SCALE changes more than the card")
    require(manual, r"Everything gets smaller by the same amount", "FRAME_SCALE affects whole image")

    for symbol, url in (
        ("ALT_URL_DEFAULT", "maps:/car/instrumentcluster"),
        ("ALT_URL_MAP", "maps:/car/instrumentcluster/map"),
        ("ALT_URL_INSTRUCTIONCARD", "maps:/car/instrumentcluster/instructioncard"),
    ):
        require(hook, r"#define\s+" + symbol + r'\s+"' + re.escape(url) + r'"', symbol)
    require(hook, r"static const char \*nav_surface_url", "one selected URL per active surface")
    require(hook, r"static void active_alt_url_copy", "one active URL")
    require(hook, r"static void add_alt_suggest_ui_urls", "three supported UI URL types")
    for token in ("leftAligned", "rightAligned", "topAligned"):
        require(hook, r'"' + token + r'"', token)
    require(hook, r"static void add_reference_viewarea", "view area geometry metadata")
    require(hook, r'\"safeArea\"', "safe area geometry metadata")
    require(player, r"uint8_t\* pY = dispBuf->pixels", "player operates on decoded frames")
    require(player, r"glDrawArrays\(GL_TRIANGLE_FAN, 0, 4\)", "single video picture rendering")

    # If future upstream implements a real card-size control, stop and reassess.
    suspicious = (
        "CLUSTERMAP_CARPLAY_CARD_SCALE",
        "CLUSTERMAP_CARPLAY_CARD_WIDTH",
        "CLUSTERMAP_CARPLAY_CARD_HEIGHT",
        "maneuverCardScale",
        "maneuverCardWidth",
        "instructionCardScale",
    )
    for keyword in suspicious:
        if keyword in config or keyword in hook:
            raise AssertionError("Review new possible card-size API: " + keyword)

    return {
        "upstream": "MacsInSpace/mib-cluster-map v1.2.14",
        "public_source_has_direct_card_percent_knob": False,
        "video_transport": "one current type-111 stream in original MIB implementation",
        "supported": {
            "view": ["full", "map", "card"],
            "card_position": ["left", "right", "top"],
            "safe_area": "geometry/layout only, changes where iOS places UI",
            "global_scale": "changes card AND other icons/text/map coverage",
            "frame_scale": "changes entire rendered picture",
        },
        "conclusion": "A one-parameter card-only 70% resize is not supported by exposed v1.2.14 controls.",
        "limit": "No real GaoDe/iPhone rendering test; undocumented APIs cannot be ruled out.",
        "car_ready": False,
    }

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source_root", help="exact upstream MIB Cluster Map v1.2.14 source checkout")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    result = audit(args.source_root)
    print(json.dumps(result, ensure_ascii=False, indent=2) if args.json else
          "PASS: verified MIB v1.2.14 has no exposed card-only scale; "
          "existing size options resize whole UI. No vehicle install.")
