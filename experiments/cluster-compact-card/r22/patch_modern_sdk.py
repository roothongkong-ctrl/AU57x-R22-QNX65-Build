#!/usr/bin/env python3
"""Modern AMap SDK lab compatibility: remove obsolete traffic bar, preserve reserved layout."""
from pathlib import Path
import sys
root=Path(sys.argv[1])/"HelloCarPlay"
def edit(name,pairs):
    p=root/name
    s=p.read_text()
    for a,b in pairs:
        if s.count(a)!=1:
            raise SystemExit("R22 modern patch anchor mismatch: "+a)
        s=s.replace(a,b,1)
    p.write_text(s)
edit("MainViewController.m",[
    ("AMapNaviTrafficBarView *trafficBarView","UIView *trafficBarView"),
    ("    self.trafficBarView.borderWidth = 2;","    self.trafficBarView.hidden = YES; // SDK 11 removed this optional view"),
    ("    [[AMapNaviDriveManager sharedInstance] addDataRepresentative:self.trafficBarView];","    // R22: obsolete traffic data representative removed; layout frame retained.")
])
edit("MainViewController.xib",[
    (' customClass="AMapNaviTrafficBarView"','')
])
print("R22 modern SDK compatibility: traffic bar removed from lab only")
