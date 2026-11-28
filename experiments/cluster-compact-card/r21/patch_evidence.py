#!/usr/bin/env python3
"""Add independent before/after captures to R20 (not recompositions)."""
from pathlib import Path
import sys
p=Path(sys.argv[1])/"HelloCarPlay/MainViewController.m"
s=p.read_text()
def replace(a,b):
    global s
    if s.count(a)!=1: raise SystemExit("R21 anchor mismatch: "+a[:80])
    s=s.replace(a,b,1)
replace("    __block BOOL captured = NO;\n    card.hidden = YES;", """    __block BOOL beforeOK = NO;
    UIImage *before = [mapRenderer imageWithActions:^(UIGraphicsImageRendererContext *c) {
        beforeOK = [self.view drawViewHierarchyInRect:self.view.bounds afterScreenUpdates:YES];
    }];
    if (!beforeOK || !before) return;
    __block BOOL captured = NO;
    card.hidden = YES;""")
replace("    finished = YES;\n    NSLog(@\"R20_NATIVE_CARD70_CAPTURED\");", """    [self.view layoutIfNeeded];
    __block BOOL afterOK = NO;
    UIImage *after = [mapRenderer imageWithActions:^(UIGraphicsImageRendererContext *c) {
        afterOK = [self.view drawViewHierarchyInRect:self.view.bounds afterScreenUpdates:YES];
    }];
    if (!afterOK || !after ||
        !CGPointEqualToPoint(anchor,self.driveView.screenAnchor) ||
        zoom != self.driveView.mapZoomLevel ||
        !CGRectEqualToRect(mapFrame,self.driveView.frame) ||
        ![UIImagePNGRepresentation(before) writeToFile:[dir stringByAppendingPathComponent:@"r21-before.png"] atomically:YES] ||
        ![UIImagePNGRepresentation(after) writeToFile:[dir stringByAppendingPathComponent:@"r21-after.png"] atomically:YES]) {
        card.transform=CGAffineTransformIdentity;
        NSLog(@"R21_NATIVE_PAIR_REJECT");
        return;
    }
    finished = YES;
    NSLog(@"R20_NATIVE_CARD70_CAPTURED");
    NSLog(@"R21_NATIVE_PAIR_CAPTURED");""")
p.write_text(s)
print("R21 independent SDK before/after capture patched")
