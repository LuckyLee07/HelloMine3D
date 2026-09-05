/*
The zlib/libpng License

Copyright (c) 2018 Arthur Brainville
Copyright (c) 2015 Andrew Fenn
Copyright (c) 2005-2010 Phillip Castaneda (pjcast -- www.wreckedgames.com)

This software is provided 'as-is', without any express or implied warranty. In no
event will the authors be held liable for any damages arising from the use of this
software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject to the
following restrictions:

    1. The origin of this software must not be misrepresented; you must not claim that
        you wrote the original software. If you use this software in a product,
        an acknowledgment in the product documentation would be appreciated
        but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
        misrepresented as being the original software.

    3. This notice may not be removed or altered from any source distribution. 
 */

// Modified by HelloMine3D: modifier event timing and native focus isolation.
#include "mac/CocoaKeyboard.h"
#include "mac/CocoaInputManager.h"
#include "mac/CocoaHelpers.h"
#include "OISException.h"
#include "OISEvents.h"

#include <Cocoa/Cocoa.h>

#include <list>
#include <string>
#include <iostream>

using namespace OIS;

//-------------------------------------------------------------------//
CocoaKeyboard::CocoaKeyboard(InputManager* creator, bool buffered, bool repeat) :
 Keyboard(creator->inputSystemName(), buffered, 0, creator)
{
	CocoaInputManager* man = static_cast<CocoaInputManager*>(mCreator);
	NSView* contentView = [man->_getWindow() contentView];
	mResponder          = [[CocoaKeyboardView alloc] initWithFrame:[contentView bounds]];
	if(!mResponder)
		OIS_EXCEPT(E_General, "CocoaKeyboardView::CocoaKeyboardView >> Error creating event responder");

	[mResponder setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
	[mResponder setOISKeyboardObj:this];
	[mResponder setUseRepeat:repeat];
	[contentView addSubview:mResponder];
	[man->_getWindow() makeFirstResponder:mResponder];

	static_cast<CocoaInputManager*>(mCreator)->_setKeyboardUsed(true);
}

//-------------------------------------------------------------------//
CocoaKeyboard::~CocoaKeyboard()
{
	if(mResponder)
	{
		[mResponder removeFromSuperview];
		[mResponder release];
		mResponder = nil;
	}

	// Free the input managers keyboard
	static_cast<CocoaInputManager*>(mCreator)->_setKeyboardUsed(false);
}

//-------------------------------------------------------------------//
void CocoaKeyboard::_initialize()
{
	mModifiers = 0;
}

//-------------------------------------------------------------------//
bool CocoaKeyboard::isKeyDown(KeyCode key) const
{
	return [mResponder isKeyDown:key];
}

//-------------------------------------------------------------------//
void CocoaKeyboard::capture()
{
	// Ogre custom main loop may not always run Cocoa's event pump.
	// Pump pending app events here so keyDown/keyUp callbacks are dispatched.
	NSEvent* event = nil;
	do
	{
		event = [NSApp nextEventMatchingMask:NSEventMaskAny
									untilDate:[NSDate distantPast]
									   inMode:NSDefaultRunLoopMode
									  dequeue:YES];
		if(event)
			[NSApp sendEvent:event];
	} while(event);

	[mResponder capture];
}

//-------------------------------------------------------------------//
std::string& CocoaKeyboard::getAsString(KeyCode key)
{
	getString = "";

	CGKeyCode deviceKeycode;

	// Convert OIS KeyCode back into device keycode
	VirtualtoOIS_KeyMap keyMap = [mResponder keyConversionMap];
	for(VirtualtoOIS_KeyMap::iterator it = keyMap.begin(); it != keyMap.end(); ++it)
	{
		if(it->second == key)
			deviceKeycode = it->first;
	}

	UniChar unicodeString[1];
	UniCharCount actualStringLength;

	CGEventSourceRef sref = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
	CGEventRef ref		  = CGEventCreateKeyboardEvent(sref, deviceKeycode, true);
	CGEventKeyboardGetUnicodeString(ref, sizeof(unicodeString) / sizeof(*unicodeString), &actualStringLength, unicodeString);
	getString = unicodeString[0];

	return getString;
}

//-------------------------------------------------------------------//
void CocoaKeyboard::setBuffered(bool buffered)
{
	mBuffered = buffered;
}

//-------------------------------------------------------------------//
void CocoaKeyboard::copyKeyStates(char keys[256]) const
{
	[mResponder copyKeyStates:keys];
}

@implementation CocoaKeyboardView

- (id)initWithFrame:(NSRect)frame
{
	self = [super initWithFrame:frame];
	if(self) {
		[self populateKeyConversion];
		memset(&KeyBuffer, 0, 256);
		memset(&DispatchedKeyBuffer, 0, 256);
		memset(&FocusReleasePending, 0, 256);
		dispatchingEvents = false;
		prevModMask = 0;

		NSNotificationCenter* notifications = [NSNotificationCenter defaultCenter];
		[notifications addObserver:self selector:@selector(inputFocusLost:)
			name:NSWindowDidResignKeyNotification object:nil];
		[notifications addObserver:self selector:@selector(inputFocusLost:)
			name:NSApplicationDidResignActiveNotification object:nil];

		// Fallback path: if this responder is not firstResponder, still mirror key events.
		__unsafe_unretained CocoaKeyboardView* weakSelf = self;
		localEventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged)
																 handler:^NSEvent* _Nullable(NSEvent* _Nonnull event) {
			CocoaKeyboardView* strongSelf = weakSelf;
			if(!strongSelf || ![strongSelf acceptsKeyboardEvent:event])
				return event;

			NSWindow* w = [event window];
			if(w && [w firstResponder] == strongSelf)
				return event;

			switch([event type])
			{
				case NSEventTypeKeyDown: [strongSelf keyDown:event]; break;
				case NSEventTypeKeyUp: [strongSelf keyUp:event]; break;
				case NSEventTypeFlagsChanged: [strongSelf flagsChanged:event]; break;
				default: break;
			}
			return event;
		}];
	}
	return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	if(localEventMonitor)
	{
		[NSEvent removeMonitor:localEventMonitor];
		localEventMonitor = nil;
	}
	[super dealloc];
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (BOOL)canBecomeKeyView
{
	return YES;
}

- (void)setOISKeyboardObj:(CocoaKeyboard*)obj
{
	oisKeyboardObj = obj;
}

- (bool)hasInputFocus
{
	return [self window] != nil && [[self window] isKeyWindow] && [NSApp isActive];
}

- (bool)acceptsKeyboardEvent:(NSEvent*)event
{
	return [self hasInputFocus] && [event window] == [self window];
}

- (void)inputFocusLost:(NSNotification*)notification
{
	if([[notification name] isEqualToString:NSApplicationDidResignActiveNotification] ||
	   [notification object] == [self window])
		[self resetForFocusLoss];
}

- (void)resetForFocusLoss
{
	// No stale down/text events may replay after refocus. Notify buffered clients
	// of releases too, so their own key state (e.g. ImGui) cannot remain held.
	for(unsigned int key = 0; key < 256; ++key)
		FocusReleasePending[key] |= KeyBuffer[key] || DispatchedKeyBuffer[key];
	memset(KeyBuffer, 0, sizeof(KeyBuffer));
	memset(DispatchedKeyBuffer, 0, sizeof(DispatchedKeyBuffer));
	pendingEvents.clear();
	prevModMask = 0;
	if(oisKeyboardObj)
		oisKeyboardObj->_getModifiers() = 0;
}

- (void)capture
{
	if(![self hasInputFocus])
		[self resetForFocusLoss];

	// If not buffered just return, we update the unbuffered automatically
	if(!oisKeyboardObj->buffered() || !oisKeyboardObj->getEventCallback())
	{
		memcpy(DispatchedKeyBuffer, KeyBuffer, 256);
		pendingEvents.clear();
		memset(FocusReleasePending, 0, sizeof(FocusReleasePending));
		return;
	}

	// Flush focus releases before any newly focused input. Clear each pending
	// bit before calling out so duplicate loss notifications remain harmless.
	dispatchingEvents = true;
	for(unsigned int key = 0; key < 256; ++key)
	{
		if(!FocusReleasePending[key])
			continue;
		FocusReleasePending[key] = 0;
		oisKeyboardObj->getEventCallback()->keyReleased(
			KeyEvent(oisKeyboardObj, static_cast<KeyCode>(key), 0));
	}
	dispatchingEvents = false;

	// Run through our event stack
	eventStack::iterator cur_it;

	dispatchingEvents = true;
	for(cur_it = pendingEvents.begin(); cur_it != pendingEvents.end(); cur_it++)
	{
		DispatchedKeyBuffer[(*cur_it).event().key] = ((*cur_it).type() != MAC_KEYUP);
		if((*cur_it).type() == MAC_KEYDOWN || (*cur_it).type() == MAC_KEYREPEAT)
			oisKeyboardObj->getEventCallback()->keyPressed((*cur_it).event());
		else if((*cur_it).type() == MAC_KEYUP)
			oisKeyboardObj->getEventCallback()->keyReleased((*cur_it).event());
	}

	dispatchingEvents = false;
	memcpy(DispatchedKeyBuffer, KeyBuffer, 256);
	pendingEvents.clear();
}

- (void)setUseRepeat:(bool)repeat
{
	useRepeat = repeat;
}

- (bool)isKeyDown:(KeyCode)key
{
	return (dispatchingEvents ? DispatchedKeyBuffer : KeyBuffer)[key];
}

- (void)copyKeyStates:(char[256])keys
{
	memcpy(keys, dispatchingEvents ? DispatchedKeyBuffer : KeyBuffer, 256);
}

- (void)populateKeyConversion
{
	// Virtual Key Map to KeyCode
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x12, KC_1));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x13, KC_2));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x14, KC_3));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x15, KC_4));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x17, KC_5));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x16, KC_6));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x1A, KC_7));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x1C, KC_8));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x19, KC_9));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x1D, KC_0));

	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x33, KC_BACK)); // might be wrong

	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x1B, KC_MINUS));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x18, KC_EQUALS));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x31, KC_SPACE));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x2B, KC_COMMA));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x2F, KC_PERIOD));

	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x2A, KC_BACKSLASH));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x2C, KC_SLASH));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x21, KC_LBRACKET));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x1E, KC_RBRACKET));

	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x35, KC_ESCAPE));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x39, KC_CAPITAL));

	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x30, KC_TAB));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x24, KC_RETURN)); // double check return/enter

	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_colon, KC_COLON));     // no colon?
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x29, KC_SEMICOLON));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x27, KC_APOSTROPHE));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x32, KC_GRAVE));

	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x0B, KC_B));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x00, KC_A));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x08, KC_C));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x02, KC_D));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x0E, KC_E));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x03, KC_F));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x05, KC_G));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x04, KC_H));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x22, KC_I));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x26, KC_J));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x28, KC_K));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x25, KC_L));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x2E, KC_M));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x2D, KC_N));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x1F, KC_O));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x23, KC_P));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x0C, KC_Q));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x0F, KC_R));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x01, KC_S));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x11, KC_T));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x20, KC_U));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x09, KC_V));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x0D, KC_W));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x07, KC_X));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x10, KC_Y));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x06, KC_Z));

	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x7A, KC_F1));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x78, KC_F2));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x63, KC_F3));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x76, KC_F4));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x60, KC_F5));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x61, KC_F6));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x62, KC_F7));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x64, KC_F8));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x65, KC_F9));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x6D, KC_F10));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x67, KC_F11));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x6F, KC_F12));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x69, KC_F13));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x6B, KC_F14));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x71, KC_F15));

	// Keypad
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x52, KC_NUMPAD0));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x53, KC_NUMPAD1));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x54, KC_NUMPAD2));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x55, KC_NUMPAD3));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x56, KC_NUMPAD4));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x57, KC_NUMPAD5));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x58, KC_NUMPAD6));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x59, KC_NUMPAD7));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x5B, KC_NUMPAD8));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x5C, KC_NUMPAD9));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x45, KC_ADD));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x4E, KC_SUBTRACT));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x41, KC_DECIMAL));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x51, KC_NUMPADEQUALS));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x4B, KC_DIVIDE));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x43, KC_MULTIPLY));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x4C, KC_NUMPADENTER));

	// Keypad with numlock off
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x73, KC_NUMPAD7));  // not sure of these
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_KP_Up, KC_NUMPAD8)); // check on a non-laptop
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_KP_Page_Up, KC_NUMPAD9));
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_KP_Left, KC_NUMPAD4));
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_KP_Begin, KC_NUMPAD5));
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_KP_Right, KC_NUMPAD6));
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_KP_End, KC_NUMPAD1));
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_KP_Down, KC_NUMPAD2));
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_KP_Page_Down, KC_NUMPAD3));
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_KP_Insert, KC_NUMPAD0));
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_KP_Delete, KC_DECIMAL));

	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x7E, KC_UP));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x7D, KC_DOWN));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x7B, KC_LEFT));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x7C, KC_RIGHT));

	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x74, KC_PGUP));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x79, KC_PGDOWN));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x73, KC_HOME));
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x77, KC_END));

	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_Print, KC_SYSRQ));        // ??
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_Scroll_Lock, KC_SCROLL)); // ??
	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_Pause, KC_PAUSE));        // ??

	//keyConversion.insert(VirtualtoOIS_KeyMap::value_type(XK_Insert, KC_INSERT));    // ??
	keyConversion.insert(VirtualtoOIS_KeyMap::value_type(0x75, KC_DELETE)); // del under help key?
}

- (void)injectEvent:(KeyCode)kc eventTime:(unsigned int)time eventType:(MacEventType)type
{
	[self injectEvent:kc eventTime:time eventType:type eventText:0];
}

- (void)injectEvent:(KeyCode)kc eventTime:(unsigned int)time eventType:(MacEventType)type eventText:(unsigned int)txt
{
	// set to 1 if this is either a keydown or repeat
	KeyBuffer[kc] = (type == MAC_KEYUP) ? 0 : 1;

	if(oisKeyboardObj->buffered() && oisKeyboardObj->getEventCallback())
		pendingEvents.push_back(CocoaKeyStackEvent(KeyEvent(oisKeyboardObj, kc, txt), type));
}

#pragma mark Key Event overrides
- (void)keyDown:(NSEvent*)theEvent
{
	if(![self acceptsKeyboardEvent:theEvent])
		return;
	unsigned short virtualKey = [theEvent keyCode];
	unsigned int time		  = (unsigned int)[theEvent timestamp];
	KeyCode kc				  = keyConversion[virtualKey];

	// Shortcut chords still produce physical key events, but no printable text.
	// Cocoa's native characters preserve ordinary Shift/Option text translation.
	NSString* characters = [theEvent characters];
	const bool shortcut = ([theEvent modifierFlags] &
		(NSEventModifierFlagControl | NSEventModifierFlagCommand)) != 0;
	const NSUInteger length = [characters length];
	if(shortcut || length == 0 || oisKeyboardObj->getTextTranslation() == OIS::Keyboard::Off)
	{
		[self injectEvent:kc eventTime:time eventType:MAC_KEYDOWN];
	}
	else if(oisKeyboardObj->getTextTranslation() == OIS::Keyboard::Unicode)
	{
		for(NSUInteger i = 0; i < length; ++i)
			[self injectEvent:kc eventTime:time eventType:MAC_KEYDOWN
				eventText:(unsigned int)[characters characterAtIndex:i]];
	}
	else
	{
		[self injectEvent:kc eventTime:time eventType:MAC_KEYDOWN
			eventText:(unsigned int)(unsigned char)[characters characterAtIndex:0]];
	}
}

- (void)keyUp:(NSEvent*)theEvent
{
	if(![self acceptsKeyboardEvent:theEvent])
		return;
	unsigned short virtualKey = [theEvent keyCode];

	KeyCode kc = keyConversion[virtualKey];
	[self injectEvent:kc eventTime:[theEvent timestamp] eventType:MAC_KEYUP];
}

- (void)flagsChanged:(NSEvent*)theEvent
{
	if(![self acceptsKeyboardEvent:theEvent])
		return;
	// Device-dependent low bits and simultaneous transitions must not prevent
	// recognition of the aggregate Cocoa modifier flags.
	const NSUInteger mods = [theEvent modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;
	const NSUInteger change = prevModMask ^ mods;
	const unsigned int time = (unsigned int)[theEvent timestamp];
	struct ModifierBinding { NSUInteger mask; KeyCode key; unsigned int oisMask; };
	const ModifierBinding bindings[] = {
		{NSEventModifierFlagShift, KC_LSHIFT, OIS::Keyboard::Shift},
		{NSEventModifierFlagOption, KC_LMENU, OIS::Keyboard::Alt},
		{NSEventModifierFlagControl, KC_LCONTROL, OIS::Keyboard::Ctrl},
		{NSEventModifierFlagCommand, KC_LWIN, 0},
		{NSEventModifierFlagFunction, KC_APPS, 0},
		{NSEventModifierFlagCapsLock, KC_CAPITAL, OIS::Keyboard::CapsLock},
		{NSEventModifierFlagNumericPad, KC_NUMLOCK, OIS::Keyboard::NumLock}
	};
	for(const ModifierBinding& binding : bindings)
	{
		if(!(change & binding.mask))
			continue;
		const bool down = (mods & binding.mask) != 0;
		if(down)
			oisKeyboardObj->_getModifiers() |= binding.oisMask;
		else
			oisKeyboardObj->_getModifiers() &= ~binding.oisMask;
		[self injectEvent:binding.key eventTime:time eventType:down ? MAC_KEYDOWN : MAC_KEYUP];
	}
	prevModMask = mods;
}

- (VirtualtoOIS_KeyMap)keyConversionMap
{
	return keyConversion;
}

@end
