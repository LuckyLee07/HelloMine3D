#include "../../src/HelloMine3D/Presentation/HudInteraction.h"
#include "../../src/HelloMine3D/GameplayInput.h"
#include <iostream>
#include <stdexcept>

int main()
{
    unsigned count = 0;
    const auto check = [&](bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
        ++count;
    };
    HudInteraction ui;
    using Page = HudInteraction::Page;
    check(!ui.ownsInput() && ui.page() == Page::Game, "ordinary startup owns no UI input");
    check(!ui.open(Page::Journal), "captured gameplay cannot accidentally activate HUD");
    check(!ui.open(Page::Map), "map requires pointer ownership");
    check(!ui.dismiss(), "Escape outside HUD remains available to pause");
    for (const auto page : {Page::Pointer, Page::Journal, Page::Map}) {
        ui.togglePointer();
        check(ui.ownsInput() && ui.page() == Page::Pointer, "Tab releases pointer");
        if (page != Page::Pointer) check(ui.open(page), "click opens requested page");
        check(!ui.open(Page::Game), "page click cannot bypass close transition");
        check(ui.ownsInput(), "all HUD pages own movement/buttons/wheel");
        check(ui.dismiss() && ui.page() == Page::Game, "Escape returns to game");
        check(!ui.dismiss(), "close is consumed once");
    }
    ui.togglePointer(); ui.open(Page::Map); ui.togglePointer();
    check(!ui.ownsInput(), "Tab from map returns to game");
    GameplayFocusGate gate;
    check(gate.isFocused(), "initial focus");
    gate.suppressUntilRelease();
    check(gate.isFocused(), "UI transition does not pretend OS focus changed");
    check(!gate.allowsWorldButtons(true), "closing click cannot mine");
    check(!gate.allowsWorldButtons(true), "held click remains blocked");
    check(!gate.acceptsLookSample(), "cursor transition look discarded");
    check(gate.acceptsLookSample(), "later look accepted");
    check(gate.allowsWorldButtons(false), "release rearms world input");
    check(gate.allowsWorldButtons(true), "new world click accepted");
    gate.setFocused(false); gate.suppressUntilRelease();
    check(!gate.allowsWorldButtons(false) && !gate.acceptsLookSample(), "UI transition cannot grant background input");
    gate.setFocused(true);
    check(!gate.allowsWorldButtons(true), "focus click cannot penetrate");
    check(gate.allowsWorldButtons(false) && !gate.acceptsLookSample(), "focus restoration honors both gates");
    std::cout << "[HUD_INTERACTION] checks=" << count << " status=PASS\n";
}
