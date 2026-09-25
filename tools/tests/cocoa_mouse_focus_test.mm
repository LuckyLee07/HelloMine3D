// Non-visible missing-mouse-up focus regression; does not claim GUI acceptance.
#import <Cocoa/Cocoa.h>
#include <OIS.h>
#include "mac/CocoaMouse.h"
#include "GameplayInput.h"
#include <iostream>
static bool syntheticAppActive=true;
@interface MouseFocusTestApplication : NSApplication
@end
@implementation MouseFocusTestApplication
- (BOOL)isActive { return syntheticAppActive; }
@end
@interface MouseFocusTestWindow : NSWindow
@property BOOL syntheticKey;
@end
@implementation MouseFocusTestWindow
- (BOOL)isKeyWindow { return self.syntheticKey; }
@end
// In-process event only; never posted to the OS input queue.
@interface MouseWheelTestEvent : NSObject
@property NSPoint nativePosition;
@end
@implementation MouseWheelTestEvent
- (NSPoint)locationInWindow { return self.nativePosition; }
- (CGFloat)deltaY { return 1; }
@end
struct Listener: OIS::MouseListener {
    unsigned int held=0, releases=0;
    int x=0, y=0, wheel=0;
    bool mouseMoved(const OIS::MouseEvent& event) override {
        x=event.state.X.abs; y=event.state.Y.abs; wheel=event.state.Z.rel; return true;
    }
    bool mousePressed(const OIS::MouseEvent&, OIS::MouseButtonID button) override {
        held |= 1u<<button; return true;
    }
    bool mouseReleased(const OIS::MouseEvent&, OIS::MouseButtonID button) override {
        held &= ~(1u<<button); ++releases; return true;
    }
};
int main() { @autoreleasepool {
    [MouseFocusTestApplication sharedApplication];
    MouseFocusTestWindow* window=[[MouseFocusTestWindow alloc]
        initWithContentRect:NSMakeRect(0,0,100,100) styleMask:NSWindowStyleMaskTitled
        backing:NSBackingStoreBuffered defer:NO];
    window.syntheticKey=YES;
    OIS::ParamList parameters;
    parameters.insert({"WINDOW",std::to_string(reinterpret_cast<uintptr_t>(window))});
    auto* manager=OIS::InputManager::createInputSystem(parameters);
    auto* mouse=static_cast<OIS::Mouse*>(manager->createInputObject(OIS::OISMouse,true));
    Listener listener; mouse->setEventCallback(&listener);
    CocoaMouseView* responder=nil;
    for(NSView* view in [[window contentView] subviews])
        if([view isKindOfClass:[CocoaMouseView class]]) responder=(CocoaMouseView*)view;
    if(!responder) return 2;
    int failures=0;
    auto check=[&](int button,const char* name,bool pass) {
        std::cout<<"[COCOA_MOUSE_FOCUS] "<<(pass?"PASS ":"FAIL ")<<name<<" button="<<button<<'\n';
        if(!pass) ++failures;
    };
    const NSEventType downTypes[]={NSEventTypeLeftMouseDown,NSEventTypeRightMouseDown,NSEventTypeOtherMouseDown};
    const NSEventType upTypes[]={NSEventTypeLeftMouseUp,NSEventTypeRightMouseUp,NSEventTypeOtherMouseUp};
    auto event=[&](NSEventType type) {
        return [NSEvent mouseEventWithType:type location:NSMakePoint(50,50) modifierFlags:0
            timestamp:1 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1];
    };
    for(int button=0;button<3;++button) {
        GameplayFocusGate gate;
        gate.allowsWorldButtons(false);
        window.syntheticKey=YES; syntheticAppActive=true;
        NSEvent* down=event(downTypes[button]);
        if(button==0) [responder mouseDown:down];
        else if(button==1) [responder rightMouseDown:down];
        else [responder otherMouseDown:down];
        mouse->capture();
        check(button,"setup-poll-and-client-held",mouse->getMouseState().buttonDown(static_cast<OIS::MouseButtonID>(button))&&(listener.held&(1u<<button)));
        mouse->setCursorCaptured(false);
        check(button,"menu-capture-policy-does-not-clear-held",mouse->getMouseState().buttonDown(static_cast<OIS::MouseButtonID>(button))&&(listener.held&(1u<<button)));
        const unsigned int oldReleases=listener.releases;
        window.syntheticKey=NO; syntheticAppActive=false; gate.setFocused(false);
        [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowDidResignKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidResignActiveNotification object:NSApp];
        mouse->capture();
        check(button,"gate-blocks-world-in-background",!gate.allowsWorldButtons(mouse->getMouseState().buttons!=0));
        check(button,"loss-clears-poll-button",mouse->getMouseState().buttons==0);
        check(button,"loss-releases-client-button",listener.held==0&&listener.releases>oldReleases);
        check(button,"duplicate-loss-emits-one-release",listener.releases==oldReleases+1);
        // Intentional missing native up: it was released while another app owned focus.
        window.syntheticKey=YES; syntheticAppActive=true; gate.setFocused(true); mouse->capture();
        check(button,"refocus-rearms-gate-with-no-physical-button",gate.allowsWorldButtons(mouse->getMouseState().buttons!=0));
        // Cleanup between cases, not part of the missing-up scenario.
        NSEvent* up=event(upTypes[button]);
        if(button==0) [responder mouseUp:up];
        else if(button==1) [responder rightMouseUp:up];
        else [responder otherMouseUp:up];
        // A missed notification is covered by inactive capture itself.
        if(button==0) [responder mouseDown:down];
        else if(button==1) [responder rightMouseDown:down];
        else [responder otherMouseDown:down];
        check(button,"fresh-click-still-reaches-client",(listener.held&(1u<<button))!=0);
        window.syntheticKey=NO; syntheticAppActive=false;
        mouse->capture();
        check(button,"inactive-capture-clears-without-notification",mouse->getMouseState().buttons==0&&listener.held==0);
        window.syntheticKey=YES; syntheticAppActive=true;
        if(button==0) [responder mouseUp:up];
        else if(button==1) [responder rightMouseUp:up];
        else [responder otherMouseUp:up];
    }
    // Scroll straight into another UI region without a preceding motion event.
    mouse->setCursorCaptured(false);
    mouse->getMouseState().width=100;
    mouse->getMouseState().height=100;
    MouseWheelTestEvent* wheelEvent=[[MouseWheelTestEvent alloc] init];
    wheelEvent.nativePosition=[responder convertPoint:NSMakePoint(20,75) toView:nil];
    [responder scrollWheel:(NSEvent*)wheelEvent];
    mouse->capture();
    check(-1,"wheel-without-motion-hits-native-position",listener.x==20&&listener.y==25);
    check(-1,"wheel-location-keeps-buffered-delta",listener.wheel==60);
    [wheelEvent release];
    mouse->setEventCallback(nullptr);
    manager->destroyInputObject(mouse); OIS::InputManager::destroyInputSystem(manager);
    [window close];
    std::cout<<"[COCOA_MOUSE_FOCUS] failures="<<failures<<'\n';
    return failures?1:0;
} }
