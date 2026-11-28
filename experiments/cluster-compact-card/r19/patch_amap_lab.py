#!/usr/bin/env python3
"""Simulator-only native AMap SDK experiment, not production AMap/CarPlay."""
from pathlib import Path
import sys
root=Path(sys.argv[1])/"HelloCarPlay"
def edit(path, pairs):
    p=root/path
    s=p.read_text()
    for a,b in pairs:
        if s.count(a)!=1:
            raise SystemExit(f"anchor mismatch: {path}: {a[:70]}")
        s=s.replace(a,b,1)
    p.write_text(s)
edit("AppDelegate.m", [
    ("    return YES;\n}", """#if TARGET_OS_SIMULATOR
    if ([[NSProcessInfo processInfo].arguments containsObject:@"-R19Lab"]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
            UIViewController *host = [UIViewController new];
            host.view.backgroundColor = UIColor.blackColor;
            self.window.rootViewController = host;
            self.mainVC = [[MainViewController alloc] initWithNibName:@"MainViewController" bundle:nil];
            [host addChildViewController:self.mainVC];
            UIView *v = self.mainVC.view;
            v.frame = CGRectMake(0,0,800,298);
            v.center = CGPointMake(CGRectGetMidX(host.view.bounds), CGRectGetMidY(host.view.bounds));
            v.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleBottomMargin;
            v.clipsToBounds = YES;
            [host.view addSubview:v];
            [self.mainVC didMoveToParentViewController:host];
            [self.window makeKeyAndVisible];
            NSLog(@"R19_LAB_VIEWPORT %.0fx%.0f points",v.bounds.size.width,v.bounds.size.height);
        });
    }
#endif
    return YES;
}""")
])
edit("MainViewController.m", [
    ("    [self updateMapViewScreenAnchor:NO];  //", "    [self updateMapViewScreenAnchor:NO];  //"), 
    ("    [self updateMapViewScreenAnchor:NO];  //", "    [self updateMapViewScreenAnchor:NO];  //"),
] if False else [
    ("    [self updateMapViewScreenAnchor:NO];  //", "    [self updateMapViewScreenAnchor:NO];  //"),
    ("#pragma mark - Interface", """#if TARGET_OS_SIMULATOR
- (void)driveManagerOnCalculateRouteSuccess:(AMapNaviDriveManager *)driveManager {
    if (![[NSProcessInfo processInfo].arguments containsObject:@"-R19Lab"]) return;
    NSLog(@"R19_ROUTE_SUCCESS");
    [driveManager startEmulatorNavi];
}
#endif

#pragma mark - Interface"""),
    ("    [self updateMapViewScreenAnchor:NO];  //", """#if TARGET_OS_SIMULATOR
    static BOOL labStarted = NO;
    if (!labStarted && [[NSProcessInfo processInfo].arguments containsObject:@"-R19Lab"]) {
        labStarted = YES;
        [self.view layoutIfNeeded];
        [self updateMapViewScreenAnchor:NO];
        [[AMapNaviDriveManager sharedInstance] calculateDriveRouteWithStartPoints:@[self.startPoint]
            endPoints:@[self.endPoint] wayPoints:nil
            drivingStrategy:AMapNaviDrivingStrategySingleDefault];
        NSLog(@"R19_ROUTE_REQUEST");
    }
#endif
    [self updateMapViewScreenAnchor:NO];  //"""),
    ("    if (naviInfo) {", """    if (naviInfo) {
#if TARGET_OS_SIMULATOR
        if ([[NSProcessInfo processInfo].arguments containsObject:@"-R19Lab"]) {
            static unsigned samples = 0;
            if (++samples <= 20) NSLog(@"R19_NAV_SAMPLE %u segmentDistance=%ld",samples,(long)naviInfo.segmentRemainDistance);
        }
#endif""")
])
print("R19: SDK host viewport + official route calculation + emulator callbacks patched")
