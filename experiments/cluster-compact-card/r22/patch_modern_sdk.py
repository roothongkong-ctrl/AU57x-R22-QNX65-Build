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

# 11.x has a new route-success callback; the old delegate is not sufficient.
edit("MainViewController.m",[
    ("#pragma mark - Interface", """#if TARGET_OS_SIMULATOR
- (void)driveManager:(AMapNaviDriveManager *)manager onCalculateRouteSuccessWithType:(AMapNaviRoutePlanType)type {
    if (![[NSProcessInfo processInfo].arguments containsObject:@"-R19Lab"]) return;
    NSLog(@"R19_ROUTE_SUCCESS modern type=%ld",(long)type);
    [manager startEmulatorNavi];
}
- (void)driveManager:(AMapNaviDriveManager *)manager onCalculateRouteFailure:(NSError *)error routePlanType:(AMapNaviRoutePlanType)type {
    NSLog(@"R19_ROUTE_FAILURE domain=%@ code=%ld type=%ld",error.domain,(long)error.code,(long)type);
}
#endif

#pragma mark - Interface"""),
    ("    [[AMapNaviDriveManager sharedInstance] setDelegate:self];", """    AMapNaviDriveManager *manager = [AMapNaviDriveManager sharedInstance];
    if (!manager) { NSLog(@"R19_MANAGER_NIL"); return; }
    [manager setDelegate:self];"""),
    ("    [[AMapNaviDriveManager sharedInstance] setAllowsBackgroundLocationUpdates:YES];",
     "    [[AMapNaviDriveManager sharedInstance] setAllowsBackgroundLocationUpdates:![[NSProcessInfo processInfo].arguments containsObject:@\"-R19Lab\"]];")
])
# Keep real consent UI separate from unattended synthetic fixture. Do not grant
# location permission or assert a real person's consent in the simulator lab.
# A headless run without a test authorization stops with an explicit marker.
import re
p=root/"AppDelegate.m"
s=p.read_text()
keys=re.findall(r'(?m)^    \[AMapServices sharedServices\]\.apiKey = [^\n]+;',s)
if len(keys)!=1: raise SystemExit("R22 API key anchor mismatch")
key=keys[0].strip()
s=s.replace(keys[0],"",1)
s=s.replace('#import "MainViewController.h"','#import "MainViewController.h"\n#import <AMapNaviKit/AMapNaviKit.h>',1)
a="            self.mainVC = [[MainViewController alloc] initWithNibName:@\"MainViewController\" bundle:nil];"
if s.count(a)!=1: raise SystemExit("R22 lab host anchor mismatch")
s=s.replace(a,"""            [self.window makeKeyAndVisible];
            UIAlertController *notice = [UIAlertController alertControllerWithTitle:@"AMap SDK test privacy"
                message:@"Synthetic navigation on a cloud simulator; AMap receives test route coordinates and SDK/device information. AMap policy: https://lbs.amap.com/pages/privacy/. No real user location is requested."
                preferredStyle:UIAlertControllerStyleAlert];
            [notice addAction:[UIAlertAction actionWithTitle:@"Decline" style:UIAlertActionStyleCancel handler:^(UIAlertAction *action) {
                NSLog(@"R19_PRIVACY_DECLINED");
            }]];
            [notice addAction:[UIAlertAction actionWithTitle:@"Agree and test" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
                [[AMapNaviManagerConfig sharedConfig] updatePrivacyShow:AMapPrivacyShowStatusDidShow privacyInfo:AMapPrivacyInfoStatusDidContain];
                [[AMapNaviManagerConfig sharedConfig] updatePrivacyAgree:AMapPrivacyAgreeStatusDidAgree];
                """+key+"""
                NSLog(@"R19_PRIVACY_ACCEPTED");
"""+a,1)
tail='            NSLog(@"R19_LAB_VIEWPORT %.0fx%.0f points",v.bounds.size.width,v.bounds.size.height);'
if s.count(tail)!=1: raise SystemExit("R22 lab tail mismatch")
s=s.replace(tail,tail+"""
            }]];
            [host presentViewController:notice animated:NO completion:^{
                NSLog(@"R19_PRIVACY_AWAITING_INTERACTION");
            }];""",1)
# For the original non-lab sample, do not silently initialize without consent.
# It is not used by this experiment.
p.write_text(s)
print("R22 modern SDK real consent prompt and route callback diagnostics integrated; unattended run may require interaction")
