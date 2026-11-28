#!/usr/bin/env python3
"""R20: use the official SDK's actual card view, not a fabricated 112 stream."""
from pathlib import Path
import sys
p=Path(sys.argv[1])/"HelloCarPlay/MainViewController.m"
s=p.read_text()
def replace(a,b):
    global s
    if s.count(a)!=1: raise SystemExit("R20 anchor mismatch: "+a[:80])
    s=s.replace(a,b,1)
# Preserve original layout reservation despite transformed card.frame.
replace("self.topInfoBgView.frame.origin.x + self.topInfoBgView.frame.size.width",
        "self.topInfoBgView.center.x + self.topInfoBgView.bounds.size.width / 2.0")
replace("self.topInfoBgView.frame.origin.x + self.topInfoBgView.bounds.size.width",
        "self.topInfoBgView.center.x + self.topInfoBgView.bounds.size.width / 2.0")
replace("#pragma mark - Utility", """#if TARGET_OS_SIMULATOR
- (void)r20CaptureAndScaleCard {
    if (![[NSProcessInfo processInfo].arguments containsObject:@"-R19Lab"]) return;
    static BOOL finished = NO;
    if (finished || !self.view.window || self.topInfoBgView.alpha < 0.99 ||
        self.topInfoBgView.hidden || !self.topTurnImageView.image ||
        !self.topRoadLabel.text.length || !self.topRemainLabel.text.length) return;
    [self.view layoutIfNeeded];
    UIView *card = self.topInfoBgView;
    CGRect roi = [card convertRect:card.bounds toView:self.view];
    if (!CGRectContainsRect(self.view.bounds, roi) ||
        !CGSizeEqualToSize(self.view.bounds.size, CGSizeMake(800,298))) return;
    if (roi.size.width < 16 || roi.size.height < 16) return;
    // Only use integer-aligned original bounds in pixel oracle.
    if (!CGRectEqualToRect(roi, CGRectIntegral(roi))) return;
    CGPoint anchor = self.driveView.screenAnchor;
    CGFloat zoom = self.driveView.mapZoomLevel;
    CGRect mapFrame = self.driveView.frame;
    UIGraphicsImageRendererFormat *fmt = [UIGraphicsImageRendererFormat defaultFormat];
    fmt.scale = 1.0;
    fmt.opaque = NO;
    UIGraphicsImageRenderer *cardRenderer = [[UIGraphicsImageRenderer alloc]
        initWithSize:card.bounds.size format:fmt];
    UIImage *cardImage = [cardRenderer imageWithActions:^(UIGraphicsImageRendererContext *c) {
        [card.layer renderInContext:c.CGContext];
    }];
    UIGraphicsImageRenderer *mapRenderer = [[UIGraphicsImageRenderer alloc]
        initWithSize:self.view.bounds.size format:fmt];
    __block BOOL captured = NO;
    card.hidden = YES;
    UIImage *base = [mapRenderer imageWithActions:^(UIGraphicsImageRendererContext *c) {
        captured = [self.view drawViewHierarchyInRect:self.view.bounds afterScreenUpdates:YES];
    }];
    card.hidden = NO;
    if (!captured || !base || !cardImage) return;
    // Scale just the native card, preserving its top-left and map layout.
    [UIView performWithoutAnimation:^{
        card.transform = CGAffineTransformMake(0.7,0,0,0.7,
                  -0.15*card.bounds.size.width,-0.15*card.bounds.size.height);
    }];
    CGRect scaled = [card convertRect:card.bounds toView:self.view];
    BOOL geometry = fabs(scaled.origin.x-roi.origin.x)<0.1 &&
        fabs(scaled.origin.y-roi.origin.y)<0.1 &&
        fabs(scaled.size.width-0.7*roi.size.width)<0.1 &&
        fabs(scaled.size.height-0.7*roi.size.height)<0.1 &&
        CGPointEqualToPoint(anchor,self.driveView.screenAnchor) &&
        zoom==self.driveView.mapZoomLevel &&
        CGRectEqualToRect(mapFrame,self.driveView.frame);
    if (!geometry) {
        card.transform = CGAffineTransformIdentity;
        NSLog(@"R20_GEOMETRY_REJECT");
        return;
    }
    NSString *dir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    BOOL ok = [UIImagePNGRepresentation(base) writeToFile:[dir stringByAppendingPathComponent:@"r20-base.png"] atomically:YES] &&
        [UIImagePNGRepresentation(cardImage) writeToFile:[dir stringByAppendingPathComponent:@"r20-card.png"] atomically:YES];
    NSDictionary *meta = @{@"x":@(roi.origin.x),@"y":@(roi.origin.y),
        @"w":@(roi.size.width),@"h":@(roi.size.height),@"percent":@70,
        @"viewportPixels":@[@800,@298],@"mapGeometryUnchanged":@YES,
        @"source":@"official SDK native card, not production type112"};
    NSData *json = [NSJSONSerialization dataWithJSONObject:meta options:0 error:NULL];
    ok = ok && [json writeToFile:[dir stringByAppendingPathComponent:@"r20-meta.json"] atomically:YES];
    if (!ok) {card.transform=CGAffineTransformIdentity;return;}
    finished = YES;
    NSLog(@"R20_NATIVE_CARD70_CAPTURED");
}
#endif

#pragma mark - Utility""")
# Callback may arrive before turn-icon callback: retry on each actual nav update.
replace("self.routeRemainInfoLabel.text = [NSString stringWithFormat:@\"剩余 %@ %@\",remainDis,remainTime];",
"""self.routeRemainInfoLabel.text = [NSString stringWithFormat:@"剩余 %@ %@",remainDis,remainTime];
#if TARGET_OS_SIMULATOR
        dispatch_async(dispatch_get_main_queue(), ^{ [self r20CaptureAndScaleCard]; });
#endif""")
replace('#import "MainViewController.h"','#import "MainViewController.h"\n#import <math.h>')
p.write_text(s)
print("R20 actual SDK card scale and 1x native snapshot capture integrated")
