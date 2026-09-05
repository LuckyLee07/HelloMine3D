// Synthetic, non-visible Cocoa input regression. Not normal-window acceptance.
#import <Cocoa/Cocoa.h>
#include <OIS.h>
#include <vector>
#include <iostream>
struct Snapshot { OIS::KeyCode key; bool pressed, command, control, shift; unsigned int text; };
struct Listener : OIS::KeyListener {
    OIS::Keyboard* keyboard;
    std::vector<Snapshot> events;
    bool record(const OIS::KeyEvent& e, bool pressed) {
        events.push_back({e.key, pressed, keyboard->isKeyDown(OIS::KC_LWIN),
            keyboard->isKeyDown(OIS::KC_LCONTROL), keyboard->isKeyDown(OIS::KC_LSHIFT), e.text});
        return true;
    }
    bool keyPressed(const OIS::KeyEvent& e) override { return record(e,true); }
    bool keyReleased(const OIS::KeyEvent& e) override { return record(e,false); }
};
int main() { @autoreleasepool {
    [NSApplication sharedApplication];
    NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,100,100)
        styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    OIS::ParamList parameters;
    parameters.insert({"WINDOW",std::to_string(reinterpret_cast<uintptr_t>(window))});
    auto* manager=OIS::InputManager::createInputSystem(parameters);
    auto* keyboard=static_cast<OIS::Keyboard*>(manager->createInputObject(OIS::OISKeyboard,true));
    Listener listener; listener.keyboard=keyboard; keyboard->setEventCallback(&listener);
    auto event=[&](NSEventType type,NSUInteger flags,unsigned short key) {
        return [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:flags
            timestamp:1 windowNumber:[window windowNumber] context:nil characters:@"a"
            charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:key];
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
        [responder keyDown:nativeText];
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
    manager->destroyInputObject(keyboard); OIS::InputManager::destroyInputSystem(manager);
    [window close];
    std::cout << "[COCOA_KEYBOARD] failures=" << failures << '\n';
    return failures?1:0;
} }
