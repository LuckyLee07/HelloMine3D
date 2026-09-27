// Isolated native clipboard integration; never reads or overwrites the user's board.
#import <Cocoa/Cocoa.h>
#include "Ogre/OgreMacClipboard.h"
#include <iostream>

int main() { @autoreleasepool {
    NSPasteboard* board = [NSPasteboard pasteboardWithUniqueName];
    int failures = 0;
    auto check = [&](const char* name, bool pass) {
        std::cout << "[COCOA_CLIPBOARD] " << (pass ? "PASS " : "FAIL ") << name << '\n';
        if (!pass) ++failures;
    };
    std::string text;
    check("unicode-roundtrip", OgreMacClipboard::writeText(board, u8"山脚路口 Ridge") &&
        OgreMacClipboard::readText(board, text) && text == u8"山脚路口 Ridge");
    check("invalid-utf8-preserves-board", !OgreMacClipboard::writeText(board, "\xc0\xaf") &&
        OgreMacClipboard::readText(board, text) && text == u8"山脚路口 Ridge");
    const std::string oversized(65537, 'x');
    check("oversized-write-preserves-board", !OgreMacClipboard::writeText(board, oversized.c_str()) &&
        OgreMacClipboard::readText(board, text) && text == u8"山脚路口 Ridge");
    const std::string boundary(65536, 'x');
    check("text-byte-limit-roundtrip", OgreMacClipboard::writeText(board, boundary.c_str()) &&
        OgreMacClipboard::readText(board, text) && text == boundary);
    [board clearContents];
    NSString* nulText = [[[NSString alloc] initWithBytes:"a\0b" length:3 encoding:NSUTF8StringEncoding] autorelease];
    [board setString:nulText forType:NSPasteboardTypeString];
    check("embedded-nul-clears-stale-text", !OgreMacClipboard::readText(board, text) && text.empty());
    [board clearContents];
    [board setString:[NSString stringWithUTF8String:oversized.c_str()] forType:NSPasteboardTypeString];
    check("oversized-read-clears-stale-text", !OgreMacClipboard::readText(board, text) && text.empty());
    [board clearContents];
    [board setData:[NSData dataWithBytes:"image" length:5] forType:NSPasteboardTypePNG];
    text = "stale";
    check("non-text-clears-stale-text", !OgreMacClipboard::readText(board, text) && text.empty());
    check("empty-text-roundtrip", OgreMacClipboard::writeText(board, "") &&
        OgreMacClipboard::readText(board, text) && text.empty());
    [board releaseGlobally];
    std::cout << "[COCOA_CLIPBOARD] failures=" << failures << '\n';
    return failures ? 1 : 0;
} }
