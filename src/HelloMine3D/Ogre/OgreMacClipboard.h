#ifndef OGREMACCLIPBOARD_H_INCLUDED
#define OGREMACCLIPBOARD_H_INCLUDED

#include <string>

#ifdef __OBJC__
@class NSPasteboard;
#endif

namespace OgreMacClipboard {
// Install only after creating the ImGui context. No clipboard is read here.
void install();

#ifdef __OBJC__
// Separate board access so regression checks can use a private pasteboard.
bool readText(NSPasteboard* board, std::string& text);
bool writeText(NSPasteboard* board, const char* text);
#endif
}

#endif
