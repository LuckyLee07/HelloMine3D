// Read-only exact-app window lookup for macos_window_evidence.py.
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include <libproc.h>

static NSURL *processExecutable(pid_t pid) {
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, path, sizeof(path)) <= 0) return nil;
    return [[NSURL fileURLWithPath:[NSString stringWithUTF8String:path]]
        URLByResolvingSymlinksInPath];
}

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        if (argc != 3) return 2;
        NSString *bundleID = @(argv[1]);
        NSURL *expected = [[NSURL fileURLWithPath:@(argv[2])] URLByResolvingSymlinksInPath];
        NSURL *expectedExecutable = [[expected URLByAppendingPathComponent:
            @"Contents/Resources/bin/HelloMine3D"] URLByResolvingSymlinksInPath];
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
            // A normal launcher may exec the packaged binary without preserving
            // LaunchServices bundle registration. Resolve the window owner's
            // actual executable instead of guessing from its display name.
            NSURL *executable = processExecutable(pid.intValue);
            BOOL exactExecutable = [executable isEqual:expectedExecutable];
            BOOL exactBundle = [pids containsObject:pid];
            if ((!exactBundle && !exactExecutable) || [item[(NSString *)kCGWindowLayer] intValue] != 0 ||
                [bounds[@"Width"] doubleValue] <= 0 || [bounds[@"Height"] doubleValue] <= 0) continue;
            if (exactExecutable && !exactBundle) [pids addObject:pid];
            [windows addObject:@{@"id": item[(NSString *)kCGWindowNumber], @"pid": pid,
                @"bounds": bounds, @"title": item[(NSString *)kCGWindowName] ?: @"",
                @"owner": item[(NSString *)kCGWindowOwnerName] ?: @"",
                @"executable": executable.path ?: @"",
                @"matched_by": exactExecutable ? @"executable_path" : @"bundle_path"}];
        }
        NSError *error = nil;
        NSData *data = [NSJSONSerialization dataWithJSONObject:@{@"pids": pids, @"windows": windows}
            options:NSJSONWritingSortedKeys error:&error];
        if (!data) { fprintf(stderr, "%s\n", error.description.UTF8String); return 1; }
        [[NSFileHandle fileHandleWithStandardOutput] writeData:data];
    }
    return 0;
}
