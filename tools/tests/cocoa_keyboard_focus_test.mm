// Synthetic, non-visible Cocoa input regression. Not normal-window acceptance.
#import <Cocoa/Cocoa.h>
#include <OIS.h>
#include <vector>
#include <iostream>
// Synthetic focus flags only: neither window is shown or activated.
static bool syntheticAppActive = true;
@interface FocusTestApplication : NSApplication
@end
@implementation FocusTestApplication
- (BOOL)isActive { return syntheticAppActive; }
@end
@interface FocusTestWindow : NSWindow
@property BOOL syntheticKey;
@end
@implementation FocusTestWindow
- (BOOL)isKeyWindow { return self.syntheticKey; }
@end
// Hidden windows can have no window-server number. Preserve the real native
// event payload while explicitly supplying its test destination window.
@interface FocusTestEvent : NSObject
@property(assign) NSEvent* nativeEvent;
@property(assign) NSWindow* destination;
@end
@implementation FocusTestEvent
- (NSWindow*)window { return self.destination; }
- (id)forwardingTargetForSelector:(SEL)selector { return self.nativeEvent; }
@end
static NSEvent* addressedEvent(NSEvent* nativeEvent, NSWindow* window) {
    FocusTestEvent* event=[[[FocusTestEvent alloc] init] autorelease];
    event.nativeEvent=nativeEvent; event.destination=window;
    return (NSEvent*)event;
}

struct Snapshot { OIS::KeyCode key; bool pressed, command, control, shift; unsigned int text; };
struct Listener : OIS::KeyListener {
    OIS::Keyboard* keyboard;
    std::vector<Snapshot> events;
    bool clientHeld[256] = {};
    bool record(const OIS::KeyEvent& e, bool pressed) {
        clientHeld[e.key] = pressed;
        events.push_back({e.key, pressed, keyboard->isKeyDown(OIS::KC_LWIN),
            keyboard->isKeyDown(OIS::KC_LCONTROL), keyboard->isKeyDown(OIS::KC_LSHIFT), e.text});
        return true;
    }
    bool keyPressed(const OIS::KeyEvent& e) override { return record(e,true); }
    bool keyReleased(const OIS::KeyEvent& e) override { return record(e,false); }
};
int main() { @autoreleasepool {
    [FocusTestApplication sharedApplication];
    FocusTestWindow* window = [[FocusTestWindow alloc] initWithContentRect:NSMakeRect(0,0,100,100)
        styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    window.syntheticKey = YES;
    OIS::ParamList parameters;
    parameters.insert({"WINDOW",std::to_string(reinterpret_cast<uintptr_t>(window))});
    auto* manager=OIS::InputManager::createInputSystem(parameters);
    auto* keyboard=static_cast<OIS::Keyboard*>(manager->createInputObject(OIS::OISKeyboard,true));
    Listener listener; listener.keyboard=keyboard; keyboard->setEventCallback(&listener);
    auto event=[&](NSEventType type,NSUInteger flags,unsigned short key) {
        return addressedEvent([NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:flags
            timestamp:1 windowNumber:[window windowNumber] context:nil characters:@"a"
            charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:key],window);
    };
    NSResponder* responder=[window firstResponder];
    int failures=0;
    auto check=[&](const char* name,bool pass) {
        std::cout << "[COCOA_KEYBOARD] " << (pass?"PASS ":"FAIL ") << name << '\n';
        if(!pass) ++failures;
    };
    // A complete quick chord before one capture must preserve modifiers at A-down.
    [responder flagsChanged:event(NSEventTypeFlagsChanged,NSEventModifierFlagCommand|8,55)];
    [responder keyDown:event(NSEventTypeKeyDown,NSEventModifierFlagCommand|8,0)];
    [responder keyUp:event(NSEventTypeKeyUp,NSEventModifierFlagCommand|8,0)];
    [responder flagsChanged:event(NSEventTypeFlagsChanged,0,55)];
    keyboard->capture();
    bool chord=false;
    for(auto& e:listener.events) if(e.key==OIS::KC_A && e.pressed) chord=e.command;
    check("quick-command-chord-retains-event-time-state",chord);
    check("released-command-not-held-after-capture",!keyboard->isKeyDown(OIS::KC_LWIN));
    listener.events.clear();
    // Multiple aggregate modifier bits can change together; device bits are irrelevant.
    [responder flagsChanged:event(NSEventTypeFlagsChanged,NSEventModifierFlagControl|NSEventModifierFlagShift|3,59)];
    keyboard->capture();
    check("combined-modifier-transition",keyboard->isKeyDown(OIS::KC_LCONTROL)&&keyboard->isKeyDown(OIS::KC_LSHIFT));
    [responder keyDown:event(NSEventTypeKeyDown,NSEventModifierFlagControl|NSEventModifierFlagShift|3,0)];
    [responder keyUp:event(NSEventTypeKeyUp,NSEventModifierFlagControl|NSEventModifierFlagShift|3,0)];
    [responder flagsChanged:event(NSEventTypeFlagsChanged,0,59)];
    keyboard->capture();
    chord=false;
    for(auto& e:listener.events) if(e.key==OIS::KC_A && e.pressed) chord=e.control&&e.shift;
    check("held-modifiers-retained-until-queued-release",chord);
    check("combined-release-clears-state",!keyboard->isKeyDown(OIS::KC_LCONTROL)&&!keyboard->isKeyDown(OIS::KC_LSHIFT));
    for(NSUInteger modifier : {NSEventModifierFlagControl, NSEventModifierFlagCommand}) {
        listener.events.clear();
        [responder flagsChanged:event(NSEventTypeFlagsChanged,modifier,59)];
        [responder keyDown:event(NSEventTypeKeyDown,modifier,0)];
        [responder keyUp:event(NSEventTypeKeyUp,modifier,0)];
        [responder flagsChanged:event(NSEventTypeFlagsChanged,0,59)];
        keyboard->capture();
        bool found=false, noText=true;
        for(auto& e:listener.events) if(e.key==OIS::KC_A && e.pressed) { found=true; noText &= e.text==0; }
        check(modifier==NSEventModifierFlagControl ? "control-chord-does-not-insert-text" : "command-chord-does-not-insert-text",found&&noText);
    }
    keyboard->setTextTranslation(OIS::Keyboard::Unicode);
    for(NSString* characters : {@"", @"å", @"abcdefghijklmnop"}) {
        listener.events.clear();
        NSEvent* nativeText=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint
            modifierFlags:0 timestamp:2 windowNumber:[window windowNumber] context:nil
            characters:characters charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
        [responder keyDown:addressedEvent(nativeText,window)];
        [responder keyUp:event(NSEventTypeKeyUp,0,0)];
        keyboard->capture();
        std::vector<unsigned int> text;
        for(auto& e:listener.events) if(e.key==OIS::KC_A && e.pressed) text.push_back(e.text);
        bool correct=text.size()==([characters length] ? [characters length] : 1);
        for(NSUInteger i=0; correct && i<[characters length]; ++i)
            correct=text[i]==[characters characterAtIndex:i];
        if([characters length]==0) correct=correct && text[0]==0;
        check([characters length]==0 ? "empty-text-retains-physical-key" :
            ([characters length]==1 ? "native-translated-text-preserved" : "long-text-without-fixed-buffer"),correct);
    }
    auto loseFocus=[&]() {
        window.syntheticKey=NO; syntheticAppActive=false;
        auto* notifications=[NSNotificationCenter defaultCenter];
        [notifications postNotificationName:NSWindowDidResignKeyNotification object:window];
        [notifications postNotificationName:NSApplicationDidResignActiveNotification object:NSApp];
    };
    auto gainFocus=[&]() { syntheticAppActive=true; window.syntheticKey=YES; };
    auto released=[&](OIS::KeyCode key) {
        for(auto& e:listener.events) if(e.key==key && !e.pressed) return true;
        return false;
    };
    listener.events.clear();
    [responder flagsChanged:event(NSEventTypeFlagsChanged,NSEventModifierFlagControl|NSEventModifierFlagShift,59)];
    [responder keyDown:event(NSEventTypeKeyDown,0,13)]; // W-down, intentionally no up
    keyboard->capture();
    check("setup-held-key-and-modifiers",keyboard->isKeyDown(OIS::KC_W)&&keyboard->isKeyDown(OIS::KC_LCONTROL));
    check("setup-client-tracks-held-key-and-modifiers",listener.clientHeld[OIS::KC_W]&&listener.clientHeld[OIS::KC_LCONTROL]&&listener.clientHeld[OIS::KC_LSHIFT]);
    listener.events.clear();
    loseFocus();
    check("notification-immediately-clears-poll-state",!keyboard->isKeyDown(OIS::KC_W)&&!keyboard->isKeyDown(OIS::KC_LCONTROL)&&!keyboard->isKeyDown(OIS::KC_LSHIFT));
    check("notification-clears-ois-modifier-mask",!keyboard->isModifierDown(OIS::Keyboard::Ctrl)&&!keyboard->isModifierDown(OIS::Keyboard::Shift));
    keyboard->capture(); // Must still deliver releases while inactive.
    check("duplicate-loss-preserves-client-releases",released(OIS::KC_W)&&released(OIS::KC_LCONTROL)&&released(OIS::KC_LSHIFT));
    bool clientAllReleased=true;
    for(bool down:listener.clientHeld) clientAllReleased &= !down;
    check("buffered-client-key-state-released",clientAllReleased);
    bool releaseModifiersClear=true;
    for(auto& e:listener.events) releaseModifiersClear &= !e.command&&!e.control&&!e.shift;
    check("release-callbacks-see-cleared-modifiers",releaseModifiersClear);
    listener.events.clear();
    keyboard->capture();
    check("release-not-repeated-every-background-frame",listener.events.empty());
    gainFocus(); keyboard->capture();
    check("refocus-without-up-does-not-stick",!keyboard->isKeyDown(OIS::KC_W)&&listener.events.empty());

    // Lose and regain focus entirely between captures: stale down/text must cancel.
    [responder keyDown:event(NSEventTypeKeyDown,0,0)];
    loseFocus(); gainFocus();
    listener.events.clear(); keyboard->capture();
    bool staleDown=false;
    for(auto& e:listener.events) staleDown |= e.pressed;
    check("pending-down-canceled-across-fast-refocus",!staleDown&&!keyboard->isKeyDown(OIS::KC_A));

    loseFocus(); listener.events.clear();
    [responder keyDown:event(NSEventTypeKeyDown,0,13)];
    [responder flagsChanged:event(NSEventTypeFlagsChanged,NSEventModifierFlagControl,59)];
    gainFocus(); keyboard->capture();
    check("background-events-cannot-refill-buffer",listener.events.empty()&&!keyboard->isKeyDown(OIS::KC_W)&&!keyboard->isKeyDown(OIS::KC_LCONTROL));

    NSWindow* other=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,50,50)
        styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    listener.events.clear();
    NSEvent* foreign=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint
        modifierFlags:0 timestamp:3 windowNumber:[other windowNumber] context:nil
        characters:@"w" charactersIgnoringModifiers:@"w" isARepeat:NO keyCode:13];
    [responder keyDown:addressedEvent(foreign,other)]; keyboard->capture();
    check("foreign-window-event-rejected",listener.events.empty()&&!keyboard->isKeyDown(OIS::KC_W));
    [responder keyDown:event(NSEventTypeKeyDown,0,13)]; keyboard->capture();
    [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowDidResignKeyNotification object:other];
    check("foreign-window-loss-does-not-reset-own-input",keyboard->isKeyDown(OIS::KC_W));
    [responder keyUp:event(NSEventTypeKeyUp,0,13)]; keyboard->capture();
    [other close];

    // Modifier history must restart cleanly after losing an entire chord.
    [responder flagsChanged:event(NSEventTypeFlagsChanged,NSEventModifierFlagControl,59)];
    keyboard->capture(); loseFocus(); gainFocus(); keyboard->capture();
    listener.events.clear();
    [responder flagsChanged:event(NSEventTypeFlagsChanged,NSEventModifierFlagControl,59)];
    [responder keyDown:event(NSEventTypeKeyDown,NSEventModifierFlagControl,0)];
    [responder keyUp:event(NSEventTypeKeyUp,NSEventModifierFlagControl,0)];
    [responder flagsChanged:event(NSEventTypeFlagsChanged,0,59)];
    keyboard->capture();
    bool freshChord=false;
    for(auto& e:listener.events) if(e.key==OIS::KC_A&&e.pressed) freshChord=e.control&&e.text==0;
    check("fresh-chord-after-reset-retains-event-time-modifier",freshChord&&!keyboard->isKeyDown(OIS::KC_LCONTROL));
    manager->destroyInputObject(keyboard); OIS::InputManager::destroyInputSystem(manager);
    [window close];
    std::cout << "[COCOA_KEYBOARD] failures=" << failures << '\n';
    return failures?1:0;
} }
