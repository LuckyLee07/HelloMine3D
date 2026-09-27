#include "OgreMacClipboard.h"
#include <imgui.h>
#import <Cocoa/Cocoa.h>
#include <cstring>

namespace OgreMacClipboard {
namespace {
constexpr std::size_t MaxTextBytes = 64 * 1024;
}

bool readText(NSPasteboard* board, std::string& text)
{
    text.clear();
    NSString* value = [board stringForType:NSPasteboardTypeString];
    if (!value) return false;
    const NSUInteger bytes = [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    if (bytes > MaxTextBytes) return false;
    const char* utf8 = [value UTF8String];
    if (!utf8 || std::strlen(utf8) != bytes) return false;
    text.assign(utf8, bytes);
    return true;
}

bool writeText(NSPasteboard* board, const char* text)
{
    if (!text || strnlen(text, MaxTextBytes + 1) > MaxTextBytes) return false;
    NSString* value = [NSString stringWithUTF8String:text];
    if (!value) return false;
    [board clearContents];
    return [board writeObjects:@[value]];
}

void install()
{
    auto& platform = ImGui::GetPlatformIO();
    platform.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
        // The callback is called only for an explicit paste. Keep its returned
        // UTF-8 storage alive until the next request; text is at most 64 KiB.
        static std::string text;
        @autoreleasepool {
            return readText([NSPasteboard generalPasteboard], text) ? text.c_str() : nullptr;
        }
    };
    platform.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
        @autoreleasepool { writeText([NSPasteboard generalPasteboard], text); }
    };
}
}
