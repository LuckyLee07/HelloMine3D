// Observe a diagnostic launch without sending input or changing app focus.
// Usage: watcher <client-executable> <output.json> <command> [arguments...]
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include <cerrno>
#include <cstdio>
#include <libproc.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

int main(int argc, char **argv)
{
    @autoreleasepool {
        if (argc < 4) {
            std::fprintf(stderr, "Usage: %s client-executable output.json command [args...]\n", argv[0]);
            return 2;
        }
        NSString *executable = [[NSString stringWithUTF8String:argv[1]] stringByResolvingSymlinksInPath];
        NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
        NSMutableSet<NSNumber *> *clientPids = [NSMutableSet set];
        NSUInteger samples = 0, observedSamples = 0, activeSamples = 0, visibleSamples = 0;
        const double started = NSDate.date.timeIntervalSince1970;
        const pid_t child = fork();
        if (child < 0) return 2;
        if (child == 0) {
            execvp(argv[3], argv + 3);
            _exit(127);
        }
        int status = 0;
        bool complete = false;
        do {
            @autoreleasepool {
                bool observed = false;
                // A fully hidden direct launch need not register with
                // NSWorkspace. Observe the exact executable through libproc.
                const int count = proc_listallpids(nullptr, 0);
                std::vector<pid_t> pids(count > 0 ? count + 32 : 32);
                const int size = proc_listallpids(pids.data(), int(pids.size() * sizeof(pid_t)));
                for (int index = 0; index < size && index < int(pids.size()); ++index) {
                    char path[PROC_PIDPATHINFO_MAXSIZE] = {};
                    if (proc_pidpath(pids[index], path, sizeof(path)) <= 0) continue;
                    NSString *processPath = [[NSString stringWithUTF8String:path] stringByResolvingSymlinksInPath];
                    if ([processPath isEqualToString:executable]) {
                        [clientPids addObject:@(pids[index])];
                        observed = true;
                    }
                }
                ++samples;
                if (observed) ++observedSamples;
                if ([clientPids containsObject:@(workspace.frontmostApplication.processIdentifier)])
                    ++activeSamples;
                NSArray *windows = CFBridgingRelease(CGWindowListCopyWindowInfo(
                    kCGWindowListOptionOnScreenOnly, kCGNullWindowID));
                bool visible = false;
                for (NSDictionary *window in windows)
                    if ([clientPids containsObject:window[(id)kCGWindowOwnerPID]]) visible = true;
                if (visible) ++visibleSamples;
                pid_t result = waitpid(child, &status, WNOHANG);
                if (result == child) complete = true;
                else if (result < 0 && errno != EINTR) break;
            }
            if (!complete) usleep(20000);
        } while (!complete && NSDate.date.timeIntervalSince1970 - started < 150.0);

        const int exitCode = complete && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        const bool passed = complete && exitCode == 0 && observedSamples > 0 &&
                            activeSamples == 0 && visibleSamples == 0;
        NSDictionary *record = @{
            @"evidence_type": @"BACKGROUND_LAUNCH_PROCESS_OBSERVATION",
            @"normal_input": @NO, @"client_executable": executable,
            @"started_unix": @(started), @"finished_unix": @(NSDate.date.timeIntervalSince1970),
            @"samples": @(samples), @"observed_client_samples": @(observedSamples),
            @"client_pids": clientPids.allObjects, @"client_foreground_samples": @(activeSamples),
            @"client_onscreen_window_samples": @(visibleSamples), @"command_exit_code": @(exitCode),
            @"result": passed ? @"PASS" : @"FAIL",
            @"limit": @"20 ms sampling cannot exclude shorter transients; review native window-ordering guards too. No input was sent."
        };
        NSData *json = [NSJSONSerialization dataWithJSONObject:record options:NSJSONWritingPrettyPrinted error:nil];
        if (![json writeToFile:[NSString stringWithUTF8String:argv[2]] atomically:YES]) return 2;
        std::printf("[BACKGROUND_LAUNCH] %s samples=%lu observed=%lu foreground=%lu onscreen=%lu child=%d\n",
            passed ? "PASS" : "FAIL", (unsigned long)samples, (unsigned long)observedSamples,
            (unsigned long)activeSamples, (unsigned long)visibleSamples, exitCode);
        return passed ? 0 : 1;
    }
}
