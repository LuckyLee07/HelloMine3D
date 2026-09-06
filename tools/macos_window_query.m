// Read-only exact-app window lookup for macos_window_evidence.py.
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        if (argc != 3) return 2;
        NSString *bundleID = @(argv[1]);
        NSURL *expected = [[NSURL fileURLWithPath:@(argv[2])] URLByResolvingSymlinksInPath];
        NSMutableArray *pids = [NSMutableArray array];
        for (NSRunningApplication *app in [NSRunningApplication runningApplicationsWithBundleIdentifier:bundleID]) {
            if ([[app.bundleURL URLByResolvingSymlinksInPath] isEqual:expected])
                [pids addObject:@(app.processIdentifier)];
        }
        NSArray *info = CFBridgingRelease(CGWindowListCopyWindowInfo(
            kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID));
        NSMutableArray *windows = [NSMutableArray array];
        for (NSDictionary *item in info) {
            NSNumber *pid = item[(NSString *)kCGWindowOwnerPID];
            NSDictionary *bounds = item[(NSString *)kCGWindowBounds];
            if (![pids containsObject:pid] || [item[(NSString *)kCGWindowLayer] intValue] != 0 ||
                [bounds[@"Width"] doubleValue] <= 0 || [bounds[@"Height"] doubleValue] <= 0) continue;
            [windows addObject:@{@"id": item[(NSString *)kCGWindowNumber], @"pid": pid,
                @"bounds": bounds, @"title": item[(NSString *)kCGWindowName] ?: @"",
                @"owner": item[(NSString *)kCGWindowOwnerName] ?: @""}];
        }
        NSError *error = nil;
        NSData *data = [NSJSONSerialization dataWithJSONObject:@{@"pids": pids, @"windows": windows}
            options:NSJSONWritingSortedKeys error:&error];
        if (!data) { fprintf(stderr, "%s\n", error.description.UTF8String); return 1; }
        [[NSFileHandle fileHandleWithStandardOutput] writeData:data];
    }
    return 0;
}
